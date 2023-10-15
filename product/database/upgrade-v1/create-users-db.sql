 CREATE TABLE users (
         id integer primary key AUTOINCREMENT, MLPasswordHash text, PasswordHash text,
         Email text collate nocase, CreateDate date, LastLoginDate date,
         LastPasswordChangedDate date, UserName text collate nocase
     );

 CREATE UNIQUE INDEX users_UserName on users (UserName collate nocase);
 CREATE UNIQUE INDEX users_Email on users (Email collate nocase);
