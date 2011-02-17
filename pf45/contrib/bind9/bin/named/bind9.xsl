<?xml version="1.0" encoding="UTF-8"?>
<!--
 - Copyright (C) 2006-2009  Internet Systems Consortium, Inc. ("ISC")
 -
 - Permission to use, copy, modify, and/or distribute this software for any
 - purpose with or without fee is hereby granted, provided that the above
 - copyright notice and this permission notice appear in all copies.
 -
 - THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 - REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 - AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 - INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 - LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 - OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 - PERFORMANCE OF THIS SOFTWARE.
-->

<!-- $Id: bind9.xsl,v 1.19.82.2 2009/01/29 23:47:43 tbox Exp $ -->

<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns="http://www.w3.org/1999/xhtml">
  <xsl:template match="isc/bind/statistics">
    <html>
      <head>
        <style type="text/css">
body {
	font-family: sans-serif;
	background-color: #ffffff;
	color: #000000;
}

table {
	border-collapse: collapse;
}

tr.rowh {
	text-align: center;
	border: 1px solid #000000;
	background-color: #8080ff;
	color: #ffffff;
}

tr.row {
	text-align: right;
	border: 1px solid #000000;
	background-color: teal;
	color: #ffffff;
}

tr.lrow {
	text-align: left;
	border: 1px solid #000000;
	background-color: teal;
	color: #ffffff;
}

td, th {
	padding-right: 5px;
	padding-left: 5px;
}

.header h1 {
	background-color: teal;
	color: #ffffff;
	padding: 4px;
}

.content {
	background-color: #ffffff;
	color: #000000;
	padding: 4px;
}

.item {
	padding: 4px;
	align: right;
}

.value {
	padding: 4px;
	font-weight: bold;
}

div.statcounter h2 {
	text-align: center;
	font-size: large;
	border: 1px solid #000000;
	background-color: #8080ff;
	color: #ffffff;
}

div.statcounter dl {
	float: left;
	margin-top: 0;
	margin-bottom: 0;
	margin-left: 0;
	margin-right: 0;
}

div.statcounter dt {
	width: 200px;
	text-align: center;
	font-weight: bold;
	border: 0.5px solid #000000;
	background-color: #8080ff;
	color: #ffffff;
}

div.statcounter dd {
	width: 200px;
	text-align: right;
	border: 0.5px solid #000000;
	background-color: teal;
	color: #ffffff;
	margin-left: 0;
	margin-right: 0;
}

div.statcounter br {
	clear: left;
}
        </style>
        <title>BIND 9 Statistics</title>
      </head>
      <body>
	<div class="header">
	  <h1>Bind 9 Configuration and Statistics</h1>
	</div>

	<br/>

	<table>
	  <tr class="rowh"><th colspan="2">Times</th></tr>
	  <tr class="lrow">
	    <td>boot-time</td>
	    <td><xsl:value-of select="server/boot-time"/></td>
	  </tr>
	  <tr class="lrow">
	    <td>current-time</td>
	    <td><xsl:value-of select="server/current-time"/></td>
	  </tr>
	</table>

	<br/>

	<table>
	  <tr class="rowh"><th colspan="2">Incoming Requests</th></tr>
	  <xsl:for-each select="server/requests/opcode">
	    <tr class="lrow">
	      <td><xsl:value-of select="name"/></td>
	      <td><xsl:value-of select="counter"/></td>
	    </tr>
	  </xsl:for-each>
	</table>

	<br/>

	<table>
	  <tr class="rowh"><th colspan="2">Incoming Queries</th></tr>
	  <xsl:for-each select="server/queries-in/rdtype">
	    <tr class="lrow">
	      <td><xsl:value-of select="name"/></td>
	      <td><xsl:value-of select="counter"/></td>
	    </tr>
	  </xsl:for-each>
	</table>

	<br/>

	<xsl:for-each select="views/view">
	  <table>
	    <tr class="rowh">
	      <th colspan="2">Outgoing Queries from View <xsl:value-of select="name"/></th>
	    </tr>
	    <xsl:for-each select="rdtype">
	      <tr class="lrow">
		<td><xsl:value-of select="name"/></td>
		<td><xsl:value-of select="counter"/></td>
	      </tr>
	    </xsl:for-each>
	  </table>
	  <br/>
	</xsl:for-each>

	<br/>

	<div class="statcounter">
	  <h2>Server Statistics</h2>
	  <xsl:for-each select="server/nsstat">
	    <dl>
	      <dt><xsl:value-of select="name"/></dt>
	      <dd><xsl:value-of select="counter"/></dd>
	    </dl>
	  </xsl:for-each>
	  <br/>
	</div>

	<div class="statcounter">
	  <h2>Zone Maintenance Statistics</h2>
	  <xsl:for-each select="server/zonestat">
	    <dl>
	      <dt><xsl:value-of select="name"/></dt>
	      <dd><xsl:value-of select="counter"/></dd>
	    </dl>
	  </xsl:for-each>
	  <br />
	</div>

	<div class="statcounter">
	  <h2>Resolver Statistics (Common)</h2>
	  <xsl:for-each select="server/resstat">
	    <dl>
	      <dt><xsl:value-of select="name"/></dt>
	      <dd><xsl:value-of select="counter"/></dd>
	    </dl>
	  </xsl:for-each>
	  <br />
	</div>

	<xsl:for-each select="views/view">
	  <div class="statcounter">
	    <h2>Resolver Statistics for View <xsl:value-of select="name"/></h2>
	    <xsl:for-each select="resstat">
	      <dl>
		<dt><xsl:value-of select="name"/></dt>
		<dd><xsl:value-of select="counter"/></dd>
	      </dl>
	    </xsl:for-each>
	    <br />
	  </div>
	</xsl:for-each>

	<br />

	<xsl:for-each select="views/view">
	  <table>
	    <tr class="rowh">
	      <th colspan="2">Cache DB RRsets for View <xsl:value-of select="name"/></th>
	    </tr>
	    <xsl:for-each select="cache/rrset">
	      <tr class="lrow">
		<td><xsl:value-of select="name"/></td>
		<td><xsl:value-of select="counter"/></td>
	      </tr>
	    </xsl:for-each>
	  </table>
	  <br/>
	</xsl:for-each>

	<div class="statcounter">
	  <h2>Socket I/O Statistics</h2>
	  <xsl:for-each select="server/sockstat">
	    <dl>
	      <dt><xsl:value-of select="name"/></dt>
	      <dd><xsl:value-of select="counter"/></dd>
	    </dl>
	  </xsl:for-each>
	  <br/>
	</div>

	<br/>

        <xsl:for-each select="views/view">
          <table>
            <tr class="rowh">
              <th colspan="10">Zones for View <xsl:value-of select="name"/></th>
            </tr>
            <tr class="rowh">
              <th>Name</th>
              <th>Class</th>
              <th>Serial</th>
              <th>Success</th>
              <th>Referral</th>
              <th>NXRRSET</th>
              <th>NXDOMAIN</th>
              <th>Failure</th>
	      <th>XfrReqDone</th>
	      <th>XfrRej</th>
            </tr>
            <xsl:for-each select="zones/zone">
              <tr class="lrow">
                <td>
                  <xsl:value-of select="name"/>
                </td>
                <td>
                  <xsl:value-of select="rdataclass"/>
                </td>
                <td>
                  <xsl:value-of select="serial"/>
                </td>
                <td>
                  <xsl:value-of select="counters/QrySuccess"/>
                </td>
                <td>
                  <xsl:value-of select="counters/QryReferral"/>
                </td>
                <td>
                  <xsl:value-of select="counters/QryNxrrset"/>
                </td>
                <td>
                  <xsl:value-of select="counters/QryNXDOMAIN"/>
                </td>
                <td>
                  <xsl:value-of select="counters/QryFailure"/>
                </td>
                <td>
                  <xsl:value-of select="counters/XfrReqDone"/>
                </td>
                <td>
                  <xsl:value-of select="counters/XfrRej"/>
                </td>
              </tr>
            </xsl:for-each>
          </table>
          <br/>
        </xsl:for-each>

        <br/>

        <table>
          <tr class="rowh">
            <th colspan="7">Network Status</th>
          </tr>
          <tr class="rowh">
            <th>ID</th>
	    <th>Name</th>
            <th>Type</th>
            <th>References</th>
            <th>LocalAddress</th>
            <th>PeerAddress</th>
            <th>State</th>
          </tr>
          <xsl:for-each select="socketmgr/sockets/socket">
            <tr class="lrow">
              <td>
                <xsl:value-of select="id"/>
              </td>
              <td>
                <xsl:value-of select="name"/>
              </td>
              <td>
                <xsl:value-of select="type"/>
              </td>
              <td>
                <xsl:value-of select="references"/>
              </td>
              <td>
                <xsl:value-of select="local-address"/>
              </td>
              <td>
                <xsl:value-of select="peer-address"/>
              </td>
              <td>
                <xsl:for-each select="states">
                  <xsl:value-of select="."/>
                </xsl:for-each>
              </td>
            </tr>
          </xsl:for-each>
        </table>
        <br/>
        <table>
          <tr class="rowh">
            <th colspan="2">Task Manager Configuration</th>
          </tr>
          <tr class="lrow">
            <td>Thread-Model</td>
            <td>
              <xsl:value-of select="taskmgr/thread-model/type"/>
            </td>
          </tr>
          <tr class="lrow">
            <td>Worker Threads</td>
            <td>
              <xsl:value-of select="taskmgr/thread-model/worker-threads"/>
            </td>
          </tr>
          <tr class="lrow">
            <td>Default Quantum</td>
            <td>
              <xsl:value-of select="taskmgr/thread-model/default-quantum"/>
            </td>
          </tr>
          <tr class="lrow">
            <td>Tasks Running</td>
            <td>
              <xsl:value-of select="taskmgr/thread-model/tasks-running"/>
            </td>
          </tr>
        </table>
        <br/>
        <table>
          <tr class="rowh">
            <th colspan="5">Tasks</th>
          </tr>
          <tr class="rowh">
            <th>ID</th>
            <th>Name</th>
            <th>References</th>
            <th>State</th>
            <th>Quantum</th>
          </tr>
          <xsl:for-each select="taskmgr/tasks/task">
            <tr class="lrow">
              <td>
                <xsl:value-of select="id"/>
              </td>
              <td>
                <xsl:value-of select="name"/>
              </td>
              <td>
                <xsl:value-of select="references"/>
              </td>
              <td>
                <xsl:value-of select="state"/>
              </td>
              <td>
                <xsl:value-of select="quantum"/>
              </td>
            </tr>
          </xsl:for-each>
        </table>
	<br />
	<table>
          <tr class="rowh">
            <th colspan="4">Memory Usage Summary</th>
          </tr>
	  <xsl:for-each select="memory/summary/*">
	    <tr class="lrow">
	      <td><xsl:value-of select="name()"/></td>
	      <td><xsl:value-of select="."/></td>
	    </tr>
	  </xsl:for-each>
	</table>
	<br />
	<table>
          <tr class="rowh">
            <th colspan="10">Memory Contexts</th>
          </tr>
	  <tr class="rowh">
	    <th>ID</th>
	    <th>Name</th>
	    <th>References</th>
	    <th>TotalUse</th>
	    <th>InUse</th>
	    <th>MaxUse</th>
	    <th>BlockSize</th>
	    <th>Pools</th>
	    <th>HiWater</th>
	    <th>LoWater</th>
	  </tr>
	  <xsl:for-each select="memory/contexts/context">
	    <tr class="lrow">
	      <td>
		<xsl:value-of select="id"/>
	      </td>
              <td>
                <xsl:value-of select="name"/>
              </td>
	      <td>
		<xsl:value-of select="references"/>
	      </td>
	      <td>
		<xsl:value-of select="total"/>
	      </td>
	      <td>
		<xsl:value-of select="inuse"/>
	      </td>
	      <td>
		<xsl:value-of select="maxinuse"/>
	      </td>
	      <td>
		<xsl:value-of select="blocksize"/>
	      </td>
	      <td>
		<xsl:value-of select="pools"/>
	      </td>
	      <td>
		<xsl:value-of select="hiwater"/>
	      </td>
	      <td>
		<xsl:value-of select="lowater"/>
	      </td>
	    </tr>
	  </xsl:for-each>
	</table>

      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>
