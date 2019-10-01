🌈 Aurora
---------

This is a simulation of Aurora Prototype using qemu-sgx.


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
If you want to use Aurora for published work, please cite:
```
@article{DBLP:journals/corr/abs-1802-03530,
  author    = {Hongliang Liang and
               Mingyu Li and
               Qiong Zhang and
               Yue Yu and
               Lin Jiang and
               Yixiu Chen},
  title     = {Aurora: Providing Trusted System Services for Enclaves On an Untrusted
               System},
  journal   = {CoRR},
  volume    = {abs/1802.03530},
  year      = {2018},
  url       = {http://arxiv.org/abs/1802.03530},
  archivePrefix = {arXiv},
  eprint    = {1802.03530},
  timestamp = {Mon, 13 Aug 2018 16:47:43 +0200},
  biburl    = {https://dblp.org/rec/bib/journals/corr/abs-1802-03530},
  bibsource = {dblp computer science bibliography, https://dblp.org}
}
```

📃 License
----------

MIT License

💬 Contact
----------

[BUPT TSIS Lab Team](hliang@bupt.edu.cn)
