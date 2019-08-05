// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/** Implementation of a proxy for the transport.
 *
 * @file	transport_proxy.h
 * @author	sylvain.chapeland@cern.ch
*/

#ifndef edg_monitoring_transport_proxy_h
#define edg_monitoring_transport_proxy_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TR_proxy* TR_proxy_handle; /**< A handle to a proxy */

/** Proxy configuration structure */
typedef struct {
  char* server_name; /**< the server ip */
  int server_port;   /**< the server port */

  char* proxy_name; /**< proxy name */
  int proxy_port;   /**< proxy port */
} TR_proxy_configuration;

/** Start a proxy with a given configuration.
  * @param config 	: proxy configuration.
  * @return		: a handle to the proxy connexion.
*/
TR_proxy_handle TR_proxy_start(TR_proxy_configuration* config);

/** Stop the proxy
  * @param h : proxy handle.
*/
int TR_proxy_stop(TR_proxy_handle h);

#ifdef __cplusplus
}
#endif

#endif /* define edg_monitoring_transport_proxy_h */
