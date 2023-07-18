# Redis setup for the project

## Redis Server and CLI
Installation:
```
$ wget https://download.redis.io/redis-stable.tar.gz
$ tar -xzvf redis-stable.tar.gz
$ rm redis-stable.tar.gz 
$ cd redis-stable/
$ make
$ sudo make install 
```

## C client
Installation:
```
$ wget https://github.com/redis/hiredis/archive/refs/tags/v1.1.0.tar.gz
$ tar -xf v1.1.0.tar.gz
$ rm v1.1.0.tar.gz 
$ cd hiredis-1.1.0/
$ make
$ sudo make install
```

Once installation completed, run this:
```
$ sudo ldconfig
```

### Documentation
- [C Redis client](https://docs.redis.com/latest/rs/references/client_references/client_c/)
- [C Redis client on GitHub](https://github.com/redis/hiredis)
