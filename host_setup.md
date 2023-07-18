Update packages:
```
$ sudo apt-get update -y
$ sudo apt-get upgrade -y
```

Install build and SW dev tools:
```
$ sudo apt-get install -y git make gcc clang build-essential uuid uuid-dev
```

Build and install hiredis:
follow guide

Build and install redis:
follow guide, but amend `make` to `make MALLOC=libc`