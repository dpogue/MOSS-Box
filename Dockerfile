#  MOSS - A server for the Myst Online: Uru Live client/protocol
#  Copyright (C) 2024 dpogue
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

FROM ubuntu:18.04 AS run_env
RUN \
    apt-get update && \
    apt-get install -y libssl1.1 postgresql unzip zlib1g && \
    /etc/init.d/postgresql start && \
    adduser --disabled-password --no-create-home --gecos "" moss && \
    mkdir -p /opt/moss && chown -R moss /opt/moss && \
    su - postgres -c 'createuser -l moss'

FROM run_env AS builder
RUN \
    apt-get install -y libssl-dev postgresql-server-dev-all libpqxx-dev autoconf libtool zlib1g-dev && \
    mkdir -p /usr/src

COPY . /usr/src/moss
WORKDIR /usr/src

RUN \
    cd moss && \
    bash bootstrap.sh && \
    ./configure --enable-fast-download --disable-single-login --prefix=/opt/moss && \
    make && \
    make install && \
    mkdir -p /opt/moss/etc && \
    chown -R moss /opt/moss && \
    cp main.cfg /opt/moss/etc/moss.cfg && \
    cp backend.cfg /opt/moss/etc/moss_backend.cfg && \
    cd postgresql && \
    make && \
    make install

FROM run_env AS production
COPY --from=builder /opt/moss /opt/moss
COPY --from=builder /usr/share/postgresql/10/extension /usr/share/postgresql/10/extension
COPY --from=builder /usr/lib/postgresql/10/lib /usr/lib/postgresql/10/lib

USER moss
WORKDIR /opt/moss

EXPOSE 14617/tcp
