此固件还没有进行深度测试，请大家踊跃上报使用中遇到的问题。

## Z2MP Tasmota 兼容固件

此固件是在Tasmota固件的基础上，为Z2MP定制开发了必要的功能。
- 将原生的SoftwareSerialBridge，增加TCP透传功能。
- 增加ccloader刷zigbee固件功能。感谢[罗总](https://github.com/qlwz)的移植。
- 增加littlefs保存zigbee固件和script的功能，方便刷机。
- 同时包含了zigbee协调器和zigbee路由器的固件，可以在web端更新刷机。

计划将实现的功能
- [ ] 兼容Tasmota2Zigbee和zigbee2mqtt

## 固件构成

- Tasmota[固件]()
- Zigbee固件+Script脚本的[文件系统]()

## 刷机流程

### 资料准备

- 固件和文件系统
- esp刷机工具

### 固件烧写

### 配网

### Zigbee固件烧写

在控制台界面，发命令切换Zigbee固件。

|固件类型|刷机命令|
|--|--|
|协调器固件|`CCloaderUpdate cc2530_c.bin`|
|路由器固件|`CCloaderUpdate cc2530_r.bin`|

刷机大概需要2分钟，控制台输出日志大致如下
```
16:23:51 CMD: CCloaderUpdate cc2530_c.bin
16:23:51 SRC: WebConsole from 192.168.137.1
16:23:51 CMD: 组: 0, 索引: 1, 命令: "CCLOADERUPDATE", 数据: "cc2530_c.bin"
16:23:51 ZIG: Update cc253x fw cc2530_c.bin
16:23:51 ZIG: fw size 262144
16:23:51 ZIG: Block total: 512
16:23:51 ZIG: ChipID: 0xA5
16:23:51 ZIG: ChipID: Erase done
16:23:51 ZIG: Start uploader loop handler
16:23:51 RSL: RESULT = {"CCloaderUpdate":"Done"}
16:23:51 ZIG: Start upload blk 1/512
16:23:51 ZIG: upload blk 1/512 done
16:23:51 ZIG: Start upload blk 2/512
16:23:51 ZIG: upload blk 2/512 done
16:23:51 ZIG: Start upload blk 3/512
16:23:51 ZIG: upload blk 3/512 done
16:23:52 ZIG: Start upload blk 4/512
16:23:52 ZIG: upload blk 4/512 done
16:23:52 ZIG: Start upload blk 5/512
16:23:52 ZIG: upload blk 5/512 done
16:23:52 ZIG: Start upload blk 6/512
16:23:52 ZIG: upload blk 6/512 done
16:23:52 ZIG: Start upload blk 7/512
16:23:52 ZIG: upload blk 7/512 done
16:23:52 ZIG: Start upload blk 8/512
16:23:52 ZIG: upload blk 8/512 done
16:23:52 ZIG: Start upload blk 9/512
16:23:52 ZIG: upload blk 9/512 done
16:23:53 ZIG: Start upload blk 10/512
16:23:53 ZIG: upload blk 10/512 done
......
16:25:26 ZIG: Start upload blk 512/512
16:25:26 ZIG: upload blk 512/512 done
16:25:26 ZIG: Firmware update OK
```

### TCP桥接端口

默认情况下，TCP桥接端口是8880。如果需要修改，请在控制台输入下面的命令：（假设修改到8888）

```
SSer2NetPort 8888
```

## Tasmota使用教程

教程请在[官网](https://tasmota.github.io/docs/)自行学习。



#### 其他

- 官方[Readme](README_Tasmota.md)