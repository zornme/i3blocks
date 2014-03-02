all: package source

package:
	makepkg

source:
	makepkg -S

clean:
	rm -rf src/ pkg/ *.tar.gz i3blocks-*.{src.tar.gz,pkg.tar.xz}
