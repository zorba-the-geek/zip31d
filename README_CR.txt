README_CR.txt - Readme for Info-ZIP Traditional Encryption (ZCRYPT 3.0)

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Description
      -----------

This file discusses ZCRYPT 3.0, the version of Traditional (ZipCrypto)
encryption now included in the Zip and UnZip source kits.  The last
separate kit for adding Traditional encryption to Zip or UnZip is
ZCRYPT 2.9.

A modified version of Traditional (ZipCrypto) encryption (the original
encryption in PKZIP) has been available for Zip and UnZip for some time
as the separate package ZCRYPT.  As of 2002, changes to US export laws
(as detailed below) has allowed us to bundle this traditional encryption
and decryption code as part of the source code distributions from Zip
2.31 on and from UnZip 5.50 on.  This means that the separate ZCRYPT
encryption package is not needed for these later Zip 2.x and UnZip 5.x
releases.  The ZCRYPT code is also included in Zip 3.x and UnZip 6.x.

The ZCRYPT 2.9 kit is still needed for adding Traditional encryption to
earlier releases.  This kit is available on our ftp site.  More details
are provided in the file README.CR included in the ZCRYPT 2.9 package.

This file describes Info-ZIP ZCRYPT 3.0, the current implementation of
the ZCRYPT code in Zip and UnZip.  The source code consists of the files
crypt.c and crypt.h, which are already integrated in the current versions
of Zip and UnZip.  Those files and this file represent the contents of
ZCRYPT 3.0.  The traditional (ZipCrypto) algorithms in these files, which
is ZCRYPT 3.0, is essentially the same as in ZCRYPT 2.9 and is compatible
with the algorithms described in the PKWARE AppNote.  The ZCRYPT 3.0
files are now considered part of the Zip and UnZip source kits and are
singled out here mainly to allow for review of the implementation of the
traditional encryption and decryption algorithms per US government
requirements.  These files are now updated from version to version of Zip
and UnZip, and files from one version may not work in other versions.
DO NOT USE THESE FILES TO IMPLEMENT ENCRYPTION OR DECRYPTION IN ANY OTHER
VERSIONS OF Zip OR UnZip.  If you need to add Traditional zip encryption
to old versions of Zip or UnZip, look for the ZCRYPT 2.9 kit.

We do not expect to update ZCRYPT 3.0, which includes the Traditional
encryption and decryption algorithms in crypt.c and crypt.h as well as
this file, as long as those algorithms do not change.  If the algorithms
do change, this file will be updated to reflect that.

The ZCRYPT 2.9 kit should be available on the Info-ZIP ftp site:

     ftp://ftp.info-zip.org/pub/infozip/

As of ZCRYPT version 2.9, this encryption and decryption source code is
copyrighted by Info-ZIP; see the file LICENSE for details.  Older
versions of the ZCRYPT code and algorithms remain in the public domain.
See the file README.CR for more information.

The ability to export from the US is new (as of 2002) and is due to a
change in the regulations now administered by the Bureau of Industry and
Security (U.S. Department of Commerce), as published in Volume 65,
Number 10, of the Federal Register [14 January 2000].  Info-ZIP
initially filed the required notification via e-mail on 9 April 2000. 
The file USexport_orig.msg shows the original notification information.

As of June 2002, this code can now be freely distributed in both source
and object forms from the USA under License Exception TSU of the U.S.
Export Administration Regulations (section 740.13(e)) of 6 June 2002).
Export is subject to the constraints of these regulations.  Though we
believe zcrypt can now be freely distributed from any country, check
your local laws and regulations for guidance.  Distribution of the AES
strong encryption code is subject to the notice in the file README.

The latest filing for zcrypt is in USexport_zcrypt.msg and for AES
encryption in USexport_aes_wg.msg.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Implementation
      --------------

This traditional zip encryption code is a direct transcription of the
algorithm from Roger Schlafly, described originally by Phil Katz in the
file:

      http://www.pkware.com/documents/casestudies/APPNOTE.TXT

Note that this encryption will probably resist attacks by amateurs if
the password is well chosen and long enough (at least 8 characters) but
it will probably not resist attacks by experts.  A Web search should
find various discussions of zip encryption weaknesses.  Short passwords
consisting of lowercase letters only can be recovered in a few hours on
any workstation.  But for casual cryptography designed to keep your
mother from reading your mail, it may be adequate.

For stronger encryption, UnZip version 6.1 (or later) and Zip version
3.1 (or later) have optional support for AES encryption.  This strong
encryption implementation requires a separate Info-ZIP AES kit.  (The
file INSTALL in the Zip and UnZip source kits detail how to get and
install the AES kit.)  Many public-key encryption programs are also
available, including GNU Privacy Guard (GnuPG, GPG).

Zip 2.3x and UnZip 5.5x and later are compatible with PKZIP 2.04g. 
(Thanks to Phil Katz for accepting our suggested minor changes to the
zipfile format.)

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Information of Historical Interest
      ----------------------------------

   IMPORTANT NOTE:

Zip archives produced by Zip 2.0 or later must not be *updated* by
Zip 1.1 or PKZIP 1.10 or PKZIP 1.93a, if they contain encrypted members
or if they have been produced in a pipe or on a non-seekable device. The
old versions of Zip or PKZIP would destroy the zip structure.  The old
versions can list the contents of the zipfile but cannot extract it
anyway (because of the new compression algorithm).  If you do not use
encryption and compress regular disk files, you need not worry about
this problem.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Version History
      ---------------

      3.0  2015-11-05  Revised documentation.  The ZCRYPT code now
                       includes some infrastructure for AES encryption,
                       but all the actual AES code is packaged in a
                       separate Info-ZIP AES source kit.

