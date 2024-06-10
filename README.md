# sandbox

## 使用步骤

### 1. 创建空文件

```shell
fallocate -l 1g ./sandbox-file.iso
```

### 2. 格式化文件系统

```shell
sandbox -f ./sandbox-file.iso
```

### 3. 创建loop设备

```shell
# 在此之前如果确认有 /dev/loop0 设备则不必再创建
mknod /dev/loop0 b 7 0
```

### 4. 将文件关联到/dev/loop0
```shell
losetup /dev/loop0 ./xx.iso
```

### 5. 将设备挂在到指定目录
```shell
./sandbox --mount /dev/loop0 /xxx/<挂载点>
```

### 6. 卸载挂在点
```shell
umount /xxx/<挂载点>
# 或
./sandbox -u /dev/loop0
```