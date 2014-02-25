# Maintainer: Vivien Didelot <vivien+aur@didelot.org>
_ghuser=vivien
pkgname=i3blocks
pkgver=a2121e0
pkgrel=1
pkgdesc='Define blocks for your i3 status line'
arch=('any')
url="https://github.com/$_ghuser/$pkgname"
license=('GPL3')
source=("https://github.com/$_ghuser/$pkgname/tarball/$pkgver")
md5sums=('f653a2b745cf055e5628a2ec970a0e72')

build () {
  make -C "$srcdir/$_ghuser-$pkgname-$pkgver" PREFIX=/usr
}

package () {
  make -C "$srcdir/$_ghuser-$pkgname-$pkgver" DESTDIR="$pkgdir" PREFIX=/usr install
}

# vim: et ts=2 sw=2
