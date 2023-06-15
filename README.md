# final_project2

## Development environments
- linux environment (Ubuntu 20.04)
- Boost.Asio (C++ library)
- OpenSSL
- MySQL

## Quick start 
1. Open dowload this repository with `git clone https://github.com/hwangbo98/final_project2.git`
2. Create database named iot.
   ```
   CREATE DATABASE iot;
   ```
3. Create table named chat.
   ```
   USE iot;
   CREATE TABLE chat(
      no INTEGER AUTO_INCREMENT PRIMARY KEY,
      name VARCHAR(20) NOT NULL,
      room VARCHAR(20) NOT NULL,
      message text,
      date timestamp NULL DEFAULT NULL
   );
   ```
4. Run `make` to create executable file.
5. Run program on server side.
   ```
   ./chat_server 127.0.0.1 8090
   ```
6. Run program on client side .
   ```
   ./chat_client 127.0.0.1 8090 usr_name
   ```