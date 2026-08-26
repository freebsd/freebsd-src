WORLDPACKAGE=	dhclient

PKG_LICENSES=	BSD-3-Clause
PKG_SETS=	minimal minimal-jail

# dhclient uses resolvconf to update /etc/resolv.conf in case the
# DHCP server returns nameserver addresses.
PKG_DEPS.dhclient+=	resolvconf

.include <bsd.pkg.mk>
