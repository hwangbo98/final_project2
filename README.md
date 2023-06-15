# final_project2

## Development environments
- linux environment (Ubuntu 20.04)
- Boost.Asio (C++ library)
- OpenSSL
- MySQL

## Quick start 
1. Open dowload this repository with `git clone https://github.com/hwangbo98/final_project2.git`
2. Run `make` to create executable file 
3. Run program on server side
   ```
   ./chat_server 127.0.0.1 8090
   ```
4. Run program on client side 
   ```
   ./chat_client 127.0.0.1 8090 usr_name
   ```