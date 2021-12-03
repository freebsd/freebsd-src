FROM gcc:latest
WORKDIR /usr/src/unbound
RUN apt-get update
# install semantic parser & lexical analyzer
RUN apt-get install -y bison flex
# install packages used in tests
RUN apt-get install -y ldnsutils dnsutils xxd splint doxygen netcat
# accept short rsa keys, which are used in tests
RUN sed -i 's/SECLEVEL=2/SECLEVEL=1/g' /usr/lib/ssl/openssl.cnf

CMD ["/bin/bash"]
