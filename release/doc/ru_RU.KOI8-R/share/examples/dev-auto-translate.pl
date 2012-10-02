#!/usr/bin/perl
# $FreeBSD$
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/share/examples/dev-auto-translate.pl,v 1.4 2005/06/30 12:11:18 den Exp $
#
# Auto-translate some device entities from English to Russian (KOI8-R)
#
# Example:
# cd /usr/src/release/doc/ru_RU.KOI8-R
# perl share/examples/dev-auto-translate.pl -o share/xml/dev-auto-ru.sgml < ../share/xml/dev-auto.sgml
#
# This script is maintained only in HEAD branch.

use Getopt::Std;
use POSIX qw(fprintf);

$OutputFile = "";
$isOutputFile = 0;

if (getopts('o:')) {
  chomp($OutputFile = $opt_o);
  $isOutputFile = 1;
}

if($isOutputFile) {
  open TRANSLATED, "< $OutputFile"  or die "Can't open $OutputFile: $!";

  # check for already translated entities
  undef %translated;
  while(<TRANSLATED>) {
    next if !/hwlist\.([0-9a-f]+)/;
    $translated{$1} = 1;
  }

  close(TRANSLATED);

  open OUTPUTFILE, ">> $OutputFile"  or die "Can't open $OutputFile: $!";
}

# translate some entities
while (<>) {
next if !/&man\..*\.[0-9];/;
s/Controllers supported by the (&man\..*\.[0-9];) driver include:/Контроллеры, поддерживаемые драйвером $1, включая:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI controllers:/Драйвер $1 поддерживает следующие контроллеры SCSI:/;
s/The (&man\..*\.[0-9];) driver supports SCSI controllers including:/Драйвер $1 поддерживает контроллеры SCSI, включая:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI host adapters:/Драйвер $1 поддерживает следующие хост адаптеры SCSI:/;
s/The (&man\..*\.[0-9];) driver supports the following SCSI host adapter chips and SCSI controller cards:/Драйвер $1 поддерживает следующие хост адаптеры SCSI и карты контроллеров SCSI:/;
s/The (&man\..*\.[0-9];) driver supports the following:/Драйвер $1 поддерживает следующее:/;
s/The adapters currently supported by the (&man\..*\.[0-9];) driver include the following:/Адаптеры, поддерживаемые в настоящее время драйвером $1, включая следующие:/;
s/The following cards are among those supported by the (&man\..*\.[0-9];) driver:/Следующие адаптеры входят в число поддерживаемых драйвером $1:/;
s/The following cards are among those supported by the (&man\..*\.[0-9];) module:/Следующие адаптеры входят в число поддерживаемых модулем $1:/;
s/Adapters supported by the (&man\..*\.[0-9];) driver include:/Адаптеры, поддерживаемые драйвером $1, включая:/;
s/Cards supported by the (&man\..*\.[0-9];) driver include:/Карты, поддерживаемые драйвером $1, включая:/;
s/The (&man\..*\.[0-9];) driver supports the following card models:/Драйвер $1 поддерживает следующие модели карт:/;
s/The (&man\..*\.[0-9];) driver provides support for the following chipsets:/Драйвер $1 поддерживает следующие наборы микросхем:/;
s/The following NICs are known to work with the (&man\..*\.[0-9];) driver at this time:/Следующие сетевые карты работают с драйвером $1:/; 
s/The (&man\..*\.[0-9];) driver provides support for the following RAID adapters:/Драйвер $1 поддерживает следующие RAID адаптеры:/; 
s/The (&man\..*\.[0-9];) driver supports the following Ethernet NICs:/Драйвер $1 поддерживает следующие сетевые карты Ethernet:/; 
s/Cards supported by (&man\..*\.[0-9];) driver include:/Карты, поддерживаемые $1, включая:/; 
s/The (&man\..*\.[0-9];) driver supports the following adapters:/Драйвер $1 поддерживает следующие адаптеры:/; 
s/The (&man\..*\.[0-9];) driver supports the following ATA RAID controllers:/Драйвер $1 поддерживает следующие контроллеры ATA RAID:/; 
s/The following controllers are supported by the (&man\..*\.[0-9];) driver:/Драйвером $1 поддерживаются следующие контроллеры:/;
s/The (&man\..*\.[0-9];) driver supports the following cards:/Драйвер $1 поддерживает следующие карты:/;
s/The SCSI controller chips supported by the (&man\..*\.[0-9];) driver can be found onboard on many systems including:/Микросхемы SCSI контроллера, поддерживаемые драйвером $1, могут быть встроены во многие системы, включая:/;
s/The following devices are currently supported by the (&man\..*\.[0-9];) driver:/Драйвером $1 в настоящее время поддерживаются следующие устройства:/;
s/The (&man\..*\.[0-9];) driver supports cards containing any of the following chips:/Драйвер $1 поддерживает карты, содержащие любую из следующих микросхем:/;
s/The (&man\..*\.[0-9];) driver supports the following soundcards:/Драйвер $1 поддерживает следующие звуковые карты:/;
s/The (&man\..*\.[0-9];) driver supports audio devices based on the following chipset:/Драйвер $1 поддерживает звуковые устройства, основанные на следующем наборе микросхем:/;
s/The (&man\..*\.[0-9];) driver supports the following audio devices:/Драйвер $1 поддерживает следующие звуковые устройства:/;
s/The (&man\..*\.[0-9];) driver supports the following PCI sound cards:/Драйвер $1 поддерживает следующие звуковые карты PCI:/;
s/SCSI controllers supported by the (&man\..*\.[0-9];) driver include:/SCSI контроллеры, поддерживаемые драйвером $1, включают:/;
s/The (&man\..*\.[0-9];) driver supports the following SATA RAID controllers:/Драйвер $1 поддерживает следующие SATA RAID контроллеры:/;
s/Devices supported by the (&man\..*\.[0-9];) driver include:/Устройства, поддерживаемые драйвером $1, включают:/;
s/The following devices are supported by the (&man\..*\.[0-9];) driver:/Следующие устройства поддерживаются драйвером $1/;
s/The (&man\..*\.[0-9];) driver supports the following devices:/Драйвер $1 поддерживает следующие устройства:/;
s/The (&man\..*\.[0-9];) driver supports the following parallel to SCSI interfaces:/Драйвер $1 поддерживает следующие parallel to SCSI интерфейсы/;
s/The (&man\..*\.[0-9];) driver supports the following hardware:/Драйвер $1 поддерживает следующее оборудование:/;
s/The adapters supported by the (&man\..*\.[0-9];) driver include:/Адаптеры, поддерживаемые драйвером $1, включают:/;
s/The (&man\..*\.[0-9];) driver supports the following Ethernet adapters:/Драйвер $1 поддерживает следующие адаптеры Ethernet:/;
s/Controllers and cards supported by the (&man\..*\.[0-9];) driver include:/Контроллеры и карты, поддерживаемые драйвером $1, включают:/;
s/The (&man\..*\.[0-9];) driver supports the following audio chipsets:/Драйвер $1 поддерживает следующие аудио чипсеты:/;
s/The (&man\..*\.[0-9];) driver supports the following sound cards:/Драйвер $1 поддерживает следующие звуковые карты:/;
s/The (&man\..*\.[0-9];) driver provides support for the following chips:/Драйвер $1 предоставляет поддержку для следующих микросхем:/;
if($isOutputFile) {
  next if !/hwlist\.([0-9a-f]+)/;
  print OUTPUTFILE if !$translated{$1};
} else {
print;
}
}
