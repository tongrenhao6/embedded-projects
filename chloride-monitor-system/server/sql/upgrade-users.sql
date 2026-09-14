USE water_monitor;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  role ENUM('user','admin') NOT NULL DEFAULT 'user',
  display_name VARCHAR(128) NULL,
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  last_login_at DATETIME NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO users (username, password_hash, role, display_name, enabled)
VALUES
  ('admin', 'pbkdf2$120000$8ae99f44ca743d3093e2e864f613962b$13ff1d355e11ece0f36f846290d709c3a2ab372bf751ca36612156f34e22ad25', 'admin', '管理员', 1),
  ('user', 'pbkdf2$120000$7e76db9eed905ae196383b3a48406f83$85f37db28f501073d019ce93932cb5eb968566b44299cb13ded87c2dbfa98557', 'user', '普通用户', 1)
ON DUPLICATE KEY UPDATE
  role = VALUES(role),
  display_name = VALUES(display_name),
  enabled = VALUES(enabled);
