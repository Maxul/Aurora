Aurora
------

This is a simulation of Aurora Prototype using qemu-sgx

1. Install KVM-SGX
```sh
git clone https://github.com/intel/kvm-sgx
make menuconfig
make -j$(nproc)
sudo make install && sudo make install modules
```

2. Install Qemu-SGX
```sh
cd Aurora-qemu-sgx
./configure --target-list=x86_64-softmmu --disable-gtk
make -j$(nproc)
cd ..
```

3. Get Ubuntu 16.06 Image

4. Boot the kernel with Qemu-SGX
```sh
cd seabios-keyboard
sudo sh ./doit.sh
```

5. Install Aurora kernel module (inside the Ubuntu Guest)
```sh
cd Aurora-uio
make && sudo sh ./doit.sh
```

6. Run an Aurora user instance inside an SGX enclave
```sh
cd Aurora-openssh-sgx
make
./app
```
