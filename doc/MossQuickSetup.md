MOSS - Myst Online Server Software
==================================

Introduction
------------
MOSS is a UNIX-based server for the Myst Online: Uru Live client.  Development
began after the announcement of the shutdown of MOUL.  It is complete enough to
play the entire game, supports multiplayer use, is licensed under GPLv3, and is
not ported to Windows.

These instructions are intended to supplement the other documents in doc/setup,
and are primarily focused on Ubuntu Server 20.04.2 LTS.

20-03-2021.


Prerequsites (tested versions)
------------------------------
* gcc (9.3.0)
* openssl (1.1.1f)
* zlib (1.2.11)
* PostgreSQL (12.6)
* libpqxx (6.4.5 - Version 7 and above are not currently supported)
* git (to get sources)


Under Ubuntu, the following can be used to install all that is needed on a
fresh install.  Your distribution's requirements may vary.

```
sudo apt-get install build-essential libssl-dev postgresql postgresql-server-dev-all libpqxx-dev autoconf libtool zlib1g-dev unzip
```
Compile
-------

1. Create user, change use password, and make sure user owns its directory
  ```
  $ sudo useradd moss -d /opt/moss -m -s /bin/bash
  $ sudo passwd moss
  $ sudo chown -R moss:moss /opt/moss
  ```

2. Get source and change to source directory
  ```
  $ git clone https://foundry.openuru.org/gitblit/r/MOSS.git moss
  $ cd moss
  ```

3. Configure and compile
  ```
  $ ./bootstrap.sh
  $ ./configure --prefix /opt/moss --enable-fast-download
  $ make
  $ sudo make install
  ```

4. Compile and install MOSS UUID module
  ```
  $ cd postgresql
  $ make
  $ sudo make install
  ```

  If configure complains about missing headers or libraries, make sure you have
  the *development* libraries installed.  Available configure switches for
  "configure" can be viewed by typing `./configure --help`

5. Setup postgres user, create database, and install MOSS UUID function.
    ```
    $ sudo -u postgres psql
    postgres=# CREATE USER moss WITH PASSWORD '<password>';
    postgres=# CREATE DATABASE moss WITH ENCODING='UTF8' OWNER=moss TEMPLATE=template0 CONNECTION LIMIT=-1;
    postgres=# \c moss
    moss=# CREATE EXTENSION moss_uuidgen;
    moss=# \q
    ```

6. Populate MOSS database
    1. Switch to postgresql directory in source tree, if not already there.

    2.  Load database with initial schema/data
    ```
    $ sudo -u postgres psql -d moss -f moss.sql
    ```

    3..  Initialize vault with default data
    ```
    $ echo "select initvault();" | sudo -u postgres psql -d moss
    ```

7. Load optional data if desired

    If you want to load the initial MOULa data, which consists of the Sharper
    journal entries, Laxman's welcome message, and the Great Zero "blank" image,
    perform the following commands.

    1. Change to "support" directory in source tree
    2. Load Sharper journal entries
    ```
    $ sudo -u postgres psql -d moss -f sharperjournal.sql
    ```
    3. Load Great Zero imager "blank" image
    ```
    $ sudo -u postgres psql -d moss -f gzimager-moul.sql
    ```
    4. Load Laxman's KI welcome message
    ```
    $ sudo -u postgres psql -d moss -f welcome-laxman.sql
    ```

8. Setup additional directories in the MOSS application directory.
    ```
    $ sudo -u moss mkdir -p /opt/moss/{auth/{default,Python,SDL},etc,file,game/{SDL/{Garrison,Teledahn,common},age,state},log,var}
    ```

9. Create auth, game, and gatekeeper encryption keys
    ```
    $ cd /opt/moss
    $ sudo -u moss ./bin/make_cyan_dh -g 41 -s ./etc/auth_key.der -C ./etc/auth_key.c -c ./etc/auth_pub
    $ sudo -u moss ./bin/make_cyan_dh -g 73 -s ./etc/game_key.der -C ./etc/game_key.c -c ./etc/game_pub
    $ sudo -u moss ./bin/make_cyan_dh -g 4 -s ./etc/gatekeeper_key.der -C ./etc/gatekeeper_key.c -c ./etc/gatekeeper_pub
    ```

    The .der files are your private keys - do not distribute these.  The .c files
    contain the public key in C code format that you will need if you are compiling
    your own CWE client.  The .bin files contain the public key in binary format
    which you will need to use an existing CWE client (hex edit search and replace).

    The post at http://forums.openuru.org/viewtopic.php?f=94&t=1070 contains a
    script by Christian Walther that has been modified for Python 3, to convert
    the .bin files into base64 strings for the H'uru client.

10. Create a user to play the game
    1. Generate user password auth hash
    ```
    $ /opt/moss/bin/compute_auth_hash "none@example.com" "password"
    ```
    2. Create new user in database, replacing <HASH> with the result of step 1.
    ```
    $  echo "insert into accounts values ('none@example.com','none@example.com','<HASH>',uuid(),'default','f','f');" | sudo -u postgres psql -d moss
    ```
    Note: Repeat step 10 whenever you want to add a new user to your server.


11. Create a random XXTea encryption key
    ```
    $ sudo -u moss sh -c 'head -c 16 /dev/urandom > /opt/moss/auth/default/encryption.key'
    ```

    To print out the key for use in, for example, a UruManifest config file:
    ```
    $ xxd -p -e /opt/moss/auth/default/encryption.key
    ```

12. Obtain unencrypted .age and .sdl files and copy them to the game server.

    If you are using a CWE client, obtain your files from the OpenUru
    repository (https://foundry.openuru.org/gitblit/tree/?r=MOULSCRIPT-ou.git).

    If you are using a H'uru client, obtain files from the H'uru git repository
    (https://github.com/H-uru/Plasma).

    1. Copy .age files to the /opt/moss/game/age directory.
    2. Copy the .sdl files to /opt/moss/game/SDL, as per the documentation in
       the MOSS tree.
     - Copy Garrison.sdl and grsnTrnCtrDoors.sdl to /opt/moss/game/SDL/Garrison

     - Copy Teledahn.sdl, tldnPwrTwrPerisope.sdl, tldnVaporScope.sdl, and
    tldnWRCCBrain.sdl to /opt/moss/game/SDL/Teledahn

     - Copy all single age SDL files to /opt/moss/game/SDL

     - Copy all other SDL files which are not age specific to /opt/moss/game/SDL/common (example = morph.sdl)

     - Rename ahnonay.sdl Ahnonay.sdl

     - Rename ahnonaycathedral.sdl to AhnonayCathedral.sdl

13. Change to the support directory in source tree and load global SDL values.
    ```
    $ /opt/moss/bin/global_sdl_manager -h localhost -U moss -p <password> -d moss /opt/moss/game/SDL < set_to_moul.txt
    ```

14. Copy main.cfg from the source tree to /opt/moss/etc/moss.cfg, and
    backend.cfg from the source tree to /opt/moss/etc/moss_backend.cfg.

15. Configure MOSS for your environment.  The following example is minimal,
    and does not cover all configuration needs.  Review the comments in the
    configuration files for more information on the various options and their
    purposes.

     moss_backend.cfg
     ```
    log_dir = /opt/moss/log
    pid_file = /opt/moss/var/moss_backend.pid
    db_address = /var/run/postgresql
    ```
    moss.cfg
    ```
    bind_address = 192.168.10.222 (change this to your IP)
    pid_file = /opt/moss/var/moss.pid
    ```

16. Make sure "moss" user owns all files in /opt/moss
    ```
    $ sudo chown -R moss:moss /opt/moss
    ```

Server setup is now complete, and you can log in as the moss user and start the
servers.

The order in which you start the servers is important.  Start
"./bin/moss_backend" first, and then start "./bin/moss".

If you are going to use an internal client with the /LocalData switch, then
server setup is complete.  If the internal client crashes or simply closes after
the Cyan logo animation plays, make sure you do not have any encrypted SDL or
age files in your client directory, and also make sure you do not have a
python.pak file.  It appears the client will try to use python.pak if it exists,
even if the uncompiled Python files exist.

If you want to use an external client with MOSS, you will need to set up the
MOSS data server.  That process is covered in the doc/setup file.  A separate
supplemental document for this may be provided in the future.
