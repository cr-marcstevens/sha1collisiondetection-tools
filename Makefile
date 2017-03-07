all: sha1collisiondetection libcheck

.PHONY: sha1collisiondetection libcheck

sha1collisiondetection:
	[ -d ../sha1collisiondetection ] || (cd .. && git clone https://github.com/cr-marcstevens/sha1collisiondetection && cd sha1collisiondetection && make)

libcheck:
	$(MAKE) -C libcheck
		