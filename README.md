
1、编译

使用s5pv210板子

make 210_defconfig

使用rk3288板子

make 3288_defconfig

make source

make V=1


2、使用Micro SD卡启动

烧录output/images/rootfs.squashfs到Micro SD卡的第1分区

sudo dd if=rootfs.squashfs of=/dev/sdb1


3、使用tftp启动，方便调试

拷贝output/images/rootfs.squashfs到tftp目录

cp output/images/rootfs.squashfs ./../
