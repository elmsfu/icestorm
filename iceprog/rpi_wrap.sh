iceprog_dir=$(dirname $(readlink -f $0))

sudo raspi-config nonint do_spi 0
sudo $iceprog_dir/iceprog $*
sudo raspi-config nonint do_spi 1
sleep 3
echo 17 > /sys/class/gpio/export
sleep 3
echo out > /sys/class/gpio/gpio17/direction
echo 1 >  /sys/class/gpio/gpio17/value
echo 17 > /sys/class/gpio/unexport
