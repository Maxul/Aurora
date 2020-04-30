🌈 Aurora
---------

_This is a simulation of Aurora Prototype using qemu-sgx_.


💾 Install
----------
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

3. Get Ubuntu 16.04 Image

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

📢 Citation
-----------
If you want to use [Aurora](https://ieeexplore.ieee.org/abstract/document/8859293/) for your published work, please cite:
```
@article{Liang2020Aurora,
  author  = {Hongliang {Liang} and
             Mingyu {Li} and
             Yixiu {Chen} and
             Lin {Jiang} and
             Zhuosi {Xie} and
             Tianqi {Yang}},
  title   = {Establishing Trusted I/O Paths for SGX Client Systems With Aurora}, 
  journal = {IEEE Transactions on Information Forensics and Security}, 
  year    = {2020},
  volume  = {15},
  number  = {},
  pages   = {1589-1600},
  doi     = {10.1109/TIFS.2019.2945621},
  ISSN    = {1556-6021},
  month   = {},
}
```

💬 Contact
----------

* [Hongliang Liang](mailto:hliang@bupt.edu.cn)
* [Mingyu Li](mailto:lmy2010lmy@gmail.com)

📃 License
----------

MIT License
