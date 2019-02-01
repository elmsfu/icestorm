sudo raspi-config nonint do_spi 0
sudo ./iceprog ~/rgb.bin
sudo raspi-config nonint do_spi 1
echo 17 > /sys/class/gpio/export
sleep 5
echo out > /sys/class/gpio/gpio17/direction
echo 1 >  /sys/class/gpio/gpio17/value
echo 17 > /sys/class/gpio/unexport
