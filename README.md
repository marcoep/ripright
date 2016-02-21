# ripright

 RipRight is a minimal CD ripper for Linux modeled on autorip. It can run as a daemon and will automatically start ripping any CD found in the drive after which the disc will be ejected. Ripping is always to FLAC lossless audio format with tags taken from the community-maintained MusicBrainz lookup service and cover art from Amazon where possible. If a disc is unknown to MusicBrainz, the CD will be ejected without ripping and can also be optionally rejected if cover art cannot be found.

With RipRight, ripping a CD collection is just a matter of feeding your Linux PC each CD in turn and waiting while they are ripped. CDs which are immediately ejected can be checked with the MusicBrainz Picard tool which allows CD identifiers to be uploaded to the website database. Any errors or inaccuracies in the database records can also be edited on the MusicBrainz website.

-- from the original creator's website http://www.mcternan.me.uk/ripright


## Authors / License

Ripright was/is written by Michael McTernan and slightly improved by me.
RipRight is released under the GNU GPLv2. 


## Installation (Ubuntu)

Install the following apt packages:
* libflac-dev
* libmagick-dev
* libmagickwand-dev
* libcurl4-gnutls-dev
* libdiscid0-dev
* libcdparanoia-dev

Run the following commands:

	$ ./configure
	$ make
	$ make check
	$ sudo make install


