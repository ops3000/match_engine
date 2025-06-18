sudo apt install -y build-essential cmake pkg-config git
sudo apt install -y librdkafka-dev libboost-all-dev

# 推荐安装在 /usr/local，保证 CMake 能找到
cd ~
git clone https://github.com/confluentinc/librdkafka.git
cd librdkafka
# 开启C++接口
./configure --enable-cpp
make -j
sudo make install
sudo cp src-cpp/rdkafkacpp.h /usr/local/include/

sudo nano /etc/hosts
127.0.0.1   kafka