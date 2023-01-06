## asmc

The source code does not belong to me. I only edited, added and
enchanged the codes in a way that I need.

#### Example Usage with Swift

```swift
func callCFunction() {
  SMCOpen()
  readAndPrintFanRPMs()
  ...
  SMCClose()
}
```

#### Tested on

Works both on Intel and Apple Silicon based Macbooks.

Tested on
2018 15″ Intel Macbook Pro & 2021 14″ M1 Max Macbook Pro

### References

I might have looked up from other places as well. These are the ones that I can remember.

- https://developer.apple.com/documentation/kernel/iokit_fundamentals
- https://github.com/lavoiesl/osx-cpu-temp
- https://github.com/spotify/linux/blob/master/drivers/hwmon/applesmc.c
- https://stackoverflow.com/a/31033665/8937209
- https://github.com/acidanthera/VirtualSMC/blob/master/Docs/SMCSensorKeys.txt

### Source

Apple System Management Control (SMC) Tool Copyright (C) 2006

### License

This project is under [MIT License](https://github.com/mrcn04/asmc/blob/master/LICENSE)
