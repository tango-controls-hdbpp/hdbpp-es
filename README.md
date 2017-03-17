# hdbpp-es
Tango device server for the HDB++ Event Subscriber 

## building
git clone --recursive http://github.com/ELETTRA-SincrotroneTrieste/hdbpp-es.git  
cd hdbpp-es  
export TANGO_DIR=/usr/local/tango-9.2.5a  
export OMNIORB_DIR=/usr/local/omniorb-4.2.1  
export ZMQ_DIR=/usr/local/zeromq-4.0.7  
make
