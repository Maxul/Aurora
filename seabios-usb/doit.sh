sudo \
    ../Aurora-qemu-sgx/x86_64-softmmu/qemu-system-x86_64 \
    -m 2G -sgx epc=64MB \
    -enable-kvm -cpu host -smp 1,sockets=1,cores=1 -machine kernel_irqchip=split \
    -hda ../images/ubuntu16.qcow2 \
    -vga cirrus -nographic -vnc :0 \
    -bios ../seabios-usb/out/bios.bin \
    -chardev file,id=seabios,path=/tmp/bios.log \
    -device isa-debugcon,iobase=0x402,chardev=seabios \
    -device pci-virt-dev -device edu \
    -fsdev local,security_model=passthrough,id=fsdev0,path=../ \
    -device virtio-9p-pci,id=fs0,fsdev=fsdev0,mount_tag=hostshare \
    -device usb-ehci,id=ehci                   \
    -device usb-kbd
