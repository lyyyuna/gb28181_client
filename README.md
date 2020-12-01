# gb28181_client

国标 gb28181 模拟客户端

- 支持 Linux
- 支持 MacOS

## 编译

```
cd gb28281_client
./init.sh

mkdir build && cd build
cmake ..
make
```

## 运行
```
./gb28181-client --server-ip 10.200.20.26 --server-id 34020000002000000001 --server-port 5064 --device-id 31011500991320000046 --filepath xxx.264
```