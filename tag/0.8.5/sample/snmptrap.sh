#!/bin/sh
#
# This sample file generates SNMP traps for testing snmptrap.nb
#
# To run this test you will need the snmptrap command provided by the net-snmp
# project on SourceForge or a compatible utility

snmptrap -d -v 1 -c public localhost .1.3.6.1.4.1.2789.2005 localhost 6 2476317 '' .1.3.6.1.4.1.2789.2005.1 s "WWW Server Has Been Restarted"

snmptrap -v 2c -d -c public localhost '' .1.3.6.1.6.3.1.1.5.3 ifIndex i 2 ifAdminStatus i 1 ifOperStatus i 1
