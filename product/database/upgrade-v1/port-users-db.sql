ATTACH '../users.db' AS users_old;

INSERT INTO users (id, MLPasswordHash, PasswordHash, Email, CreateDate, LastLoginDate,
    LastPasswordChangedDate, UserName) SELECT id, MLPasswordHash, PasswordHash, Email, CreateDate, LastLoginDate,
    LastPasswordChangedDate, UserName FROM users_old.users;
