const crypto = require("crypto");
const { pool } = require("../db");

const PASSWORD_ITERATIONS = 120000;
const PASSWORD_KEY_LENGTH = 32;
const PASSWORD_DIGEST = "sha256";

function appError(status, code, message) {
  const error = new Error(message);
  error.status = status;
  error.code = code;
  return error;
}

function hashPassword(password, salt = crypto.randomBytes(16).toString("hex")) {
  const hash = crypto
    .pbkdf2Sync(String(password), salt, PASSWORD_ITERATIONS, PASSWORD_KEY_LENGTH, PASSWORD_DIGEST)
    .toString("hex");
  return `pbkdf2$${PASSWORD_ITERATIONS}$${salt}$${hash}`;
}

function verifyPassword(password, passwordHash) {
  const parts = String(passwordHash || "").split("$");
  if (parts.length !== 4 || parts[0] !== "pbkdf2") {
    return false;
  }

  const iterations = Number.parseInt(parts[1], 10);
  const salt = parts[2];
  const expected = Buffer.from(parts[3], "hex");
  const actual = crypto.pbkdf2Sync(
    String(password),
    salt,
    iterations,
    expected.length,
    PASSWORD_DIGEST
  );

  return expected.length === actual.length && crypto.timingSafeEqual(expected, actual);
}

function userDto(row) {
  return {
    id: row.id,
    username: row.username,
    role: row.role,
    display_name: row.display_name || row.username,
    enabled: row.enabled == null ? undefined : Boolean(row.enabled),
    last_login_at: row.last_login_at,
    created_at: row.created_at,
  };
}

async function findUserByUsername(username) {
  const [rows] = await pool.execute(
    `SELECT id, username, password_hash, role, display_name, enabled
     FROM users
     WHERE username = ?
     LIMIT 1`,
    [username]
  );
  return rows[0] || null;
}

async function authenticateUser(username, password) {
  if (!username || !password) {
    throw appError(400, "invalid_credentials", "用户名和密码不能为空");
  }

  const user = await findUserByUsername(username);
  if (!user || !user.enabled || !verifyPassword(password, user.password_hash)) {
    throw appError(401, "invalid_credentials", "用户名或密码错误");
  }

  await pool.execute("UPDATE users SET last_login_at = NOW() WHERE id = ?", [user.id]);
  return userDto(user);
}

function normalizeUsername(username) {
  const normalized = String(username || "").trim();
  if (!/^[A-Za-z0-9_]{3,32}$/.test(normalized)) {
    throw appError(400, "invalid_username", "用户名需为 3-32 位字母、数字或下划线");
  }
  return normalized;
}

function normalizeRole(role) {
  if (!["user", "admin"].includes(role)) {
    throw appError(400, "invalid_role", "角色必须是 user 或 admin");
  }
  return role;
}

function normalizePassword(password, required = false) {
  if (!password && !required) {
    return null;
  }
  if (typeof password !== "string" || password.length < 6 || password.length > 64) {
    throw appError(400, "invalid_password", "密码长度需为 6-64 位");
  }
  return password;
}

async function listUsers() {
  const [rows] = await pool.execute(
    `SELECT id, username, role, display_name, enabled, last_login_at, created_at
     FROM users
     ORDER BY id ASC`
  );
  return rows.map(userDto);
}

async function createUser({ username, password, role = "user", display_name: displayName = "" }) {
  const normalizedUsername = normalizeUsername(username);
  const normalizedPassword = normalizePassword(password, true);
  const normalizedRole = normalizeRole(role);
  const safeDisplayName = String(displayName || normalizedUsername).trim().slice(0, 128);

  try {
    const [result] = await pool.execute(
      `INSERT INTO users (username, password_hash, role, display_name, enabled)
       VALUES (?, ?, ?, ?, 1)`,
      [normalizedUsername, hashPassword(normalizedPassword), normalizedRole, safeDisplayName]
    );
    return {
      id: result.insertId,
      username: normalizedUsername,
      role: normalizedRole,
      display_name: safeDisplayName,
      enabled: true,
    };
  } catch (error) {
    if (error.code === "ER_DUP_ENTRY") {
      throw appError(409, "username_exists", "用户名已存在");
    }
    throw error;
  }
}

async function updateUser(userId, patch, actorUser) {
  const id = Number.parseInt(userId, 10);
  if (!Number.isFinite(id) || id <= 0) {
    throw appError(400, "invalid_user_id", "用户 ID 不正确");
  }

  const [rows] = await pool.execute(
    "SELECT id, username, role, display_name, enabled FROM users WHERE id = ? LIMIT 1",
    [id]
  );
  const user = rows[0];
  if (!user) {
    throw appError(404, "user_not_found", `用户 ${id} 不存在`);
  }

  const updates = [];
  const params = [];

  if (patch.role != null) {
    const role = normalizeRole(patch.role);
    if (actorUser?.id === id && role !== "admin") {
      throw appError(400, "cannot_downgrade_self", "不能把当前管理员改为普通用户");
    }
    updates.push("role = ?");
    params.push(role);
  }

  if (patch.enabled != null) {
    const enabled = patch.enabled ? 1 : 0;
    if (actorUser?.id === id && enabled === 0) {
      throw appError(400, "cannot_disable_self", "不能禁用当前登录账号");
    }
    updates.push("enabled = ?");
    params.push(enabled);
  }

  const password = normalizePassword(patch.password, false);
  if (password) {
    updates.push("password_hash = ?");
    params.push(hashPassword(password));
  }

  if (updates.length > 0) {
    params.push(id);
    await pool.execute(`UPDATE users SET ${updates.join(", ")} WHERE id = ?`, params);
  }

  const [updatedRows] = await pool.execute(
    `SELECT id, username, role, display_name, enabled, last_login_at, created_at
     FROM users
     WHERE id = ?
     LIMIT 1`,
    [id]
  );
  return userDto(updatedRows[0]);
}

async function deleteUser(userId, actorUser) {
  const id = Number.parseInt(userId, 10);
  if (!Number.isFinite(id) || id <= 0) {
    throw appError(400, "invalid_user_id", "用户 ID 不正确");
  }
  if (actorUser?.id === id) {
    throw appError(400, "cannot_delete_self", "不能删除当前登录账号");
  }

  const [rows] = await pool.execute(
    "SELECT id, username FROM users WHERE id = ? LIMIT 1",
    [id]
  );
  const user = rows[0];
  if (!user) {
    throw appError(404, "user_not_found", `用户 ${id} 不存在`);
  }

  const [result] = await pool.execute("DELETE FROM users WHERE id = ?", [id]);
  if (result.affectedRows === 0) {
    throw appError(404, "user_not_found", `用户 ${id} 不存在`);
  }

  return {
    id,
    username: user.username,
  };
}

async function updateOwnProfile(actorUser, patch) {
  const id = Number.parseInt(actorUser?.id, 10);
  if (!Number.isFinite(id) || id <= 0) {
    throw appError(401, "unauthorized", "请先登录");
  }

  const [rows] = await pool.execute(
    "SELECT id, username, password_hash, role, display_name, enabled FROM users WHERE id = ? LIMIT 1",
    [id]
  );
  const user = rows[0];
  if (!user || !user.enabled) {
    throw appError(401, "unauthorized", "账号不可用，请重新登录");
  }

  const updates = [];
  const params = [];

  if (patch.username != null) {
    const username = normalizeUsername(patch.username);
    updates.push("username = ?");
    params.push(username);
  }

  if (patch.display_name != null) {
    updates.push("display_name = ?");
    params.push(String(patch.display_name || patch.username || user.username).trim().slice(0, 128));
  }

  const password = normalizePassword(patch.password, false);
  if (password) {
    if (!verifyPassword(patch.current_password, user.password_hash)) {
      throw appError(400, "invalid_current_password", "当前密码不正确");
    }
    updates.push("password_hash = ?");
    params.push(hashPassword(password));
  }

  if (updates.length === 0) {
    return userDto(user);
  }

  try {
    params.push(id);
    await pool.execute(`UPDATE users SET ${updates.join(", ")} WHERE id = ?`, params);
  } catch (error) {
    if (error.code === "ER_DUP_ENTRY") {
      throw appError(409, "username_exists", "用户名已存在");
    }
    throw error;
  }

  const [updatedRows] = await pool.execute(
    `SELECT id, username, role, display_name, enabled, last_login_at, created_at
     FROM users
     WHERE id = ?
     LIMIT 1`,
    [id]
  );
  return userDto(updatedRows[0]);
}

async function resetForgottenPassword({ username, password }) {
  const normalizedUsername = normalizeUsername(username);
  const normalizedPassword = normalizePassword(password, true);
  const user = await findUserByUsername(normalizedUsername);

  if (!user || !user.enabled) {
    throw appError(404, "user_not_found", "用户不存在或已被禁用");
  }
  if (user.role === "admin") {
    throw appError(403, "admin_reset_forbidden", "管理员密码请通过后台维护");
  }

  await pool.execute("UPDATE users SET password_hash = ? WHERE id = ?", [
    hashPassword(normalizedPassword),
    user.id,
  ]);

  return {
    username: normalizedUsername,
  };
}

module.exports = {
  appError,
  authenticateUser,
  createUser,
  deleteUser,
  hashPassword,
  listUsers,
  resetForgottenPassword,
  updateOwnProfile,
  updateUser,
  userDto,
  verifyPassword,
};
