const crypto = require("crypto");

const SESSION_COOKIE = "ph_session";
const SESSION_TTL_MS = 8 * 60 * 60 * 1000;
const sessions = new Map();

function appError(status, code, message) {
  const error = new Error(message);
  error.status = status;
  error.code = code;
  return error;
}

function parseCookies(cookieHeader = "") {
  return cookieHeader.split(";").reduce((cookies, part) => {
    const index = part.indexOf("=");
    if (index === -1) {
      return cookies;
    }
    const key = part.slice(0, index).trim();
    const value = part.slice(index + 1).trim();
    if (key) {
      cookies[key] = decodeURIComponent(value);
    }
    return cookies;
  }, {});
}

function buildSessionCookie(value, maxAgeSeconds) {
  return [
    `${SESSION_COOKIE}=${encodeURIComponent(value)}`,
    "Path=/",
    "HttpOnly",
    "SameSite=Lax",
    `Max-Age=${maxAgeSeconds}`,
  ].join("; ");
}

function createSession(res, user) {
  const sessionId = crypto.randomBytes(32).toString("hex");
  sessions.set(sessionId, {
    user,
    expiresAt: Date.now() + SESSION_TTL_MS,
  });
  res.setHeader("Set-Cookie", buildSessionCookie(sessionId, SESSION_TTL_MS / 1000));
}

function clearSession(req, res) {
  const sessionId = parseCookies(req.headers.cookie)[SESSION_COOKIE];
  if (sessionId) {
    sessions.delete(sessionId);
  }
  res.setHeader("Set-Cookie", buildSessionCookie("", 0));
}

function getSessionUser(req) {
  const sessionId = parseCookies(req.headers.cookie)[SESSION_COOKIE];
  if (!sessionId) {
    return null;
  }

  const session = sessions.get(sessionId);
  if (!session) {
    return null;
  }

  if (session.expiresAt <= Date.now()) {
    sessions.delete(sessionId);
    return null;
  }

  session.expiresAt = Date.now() + SESSION_TTL_MS;
  return session.user;
}

function attachUser(req, res, next) {
  req.user = getSessionUser(req);
  next();
}

function requireLogin(req, res, next) {
  if (!req.user) {
    next(appError(401, "unauthorized", "请先登录"));
    return;
  }
  next();
}

function requireAdmin(req, res, next) {
  if (!req.user) {
    next(appError(401, "unauthorized", "请先登录"));
    return;
  }
  if (req.user.role !== "admin") {
    next(appError(403, "forbidden", "普通用户只有查看权限"));
    return;
  }
  next();
}

module.exports = {
  attachUser,
  clearSession,
  createSession,
  requireAdmin,
  requireLogin,
};
