export CROSS_COMPILE
MEM_TYPE = MEM_DDR1
export MEM_TYPE

BOARD = stmp378x_dev
ARCH = mx23

#all: boot_prep power_prep holitest holitest_bootstream

holitest_bootstream: boot_prep power_prep holitest holitest.bd
	@echo "generating holitest boot stream image"
	elftosb -z -c ./holitest.bd -o holitest.sb

holitest:
	@echo "build holitest"
	$(MAKE) -C holitest  ARCH=$(ARCH) BOARD=$(BOARD)

power_prep:
	@echo "build power_prep"
	$(MAKE) -C power_prep ARCH=$(ARCH) BOARD=$(BOARD)

boot_prep:
	@echo "build boot_prep"
	$(MAKE) -C boot_prep  ARCH=$(ARCH) BOARD=$(BOARD)

distclean: clean
clean:
	-rm -rf *.sb
	$(MAKE) -C linux_prep clean ARCH=$(ARCH)
	$(MAKE) -C boot_prep clean ARCH=$(ARCH)
	$(MAKE) -C power_prep clean ARCH=$(ARCH)
	$(MAKE) -C holitest clean ARCH=$(ARCH)

.PHONY: all linux_prep boot_prep power_prep distclean clean holitest

