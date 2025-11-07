CREATE DATABASE IF NOT EXISTS kv_store_db; -- creating database

CREATE USER IF NOT EXISTS 'mysql_user'@'127.0.0.1' IDENTIFIED BY 'abc@123'; -- creating new user

GRANT ALL PRIVILEGES ON kv_store_db.* TO 'mysql_user'@'127.0.0.1'; -- granting privileges

FLUSH PRIVILEGES;

USE kv_store_db;

CREATE TABLE key_value (key_name VARCHAR(255) PRIMARY KEY, value TEXT);