/*
 * CYD Game Clock - WiFi Setup Portal
 *
 * Runs when no WiFi credentials are stored: a password-protected access point
 * with a captive web page for choosing the home network. The credentials are
 * tested before they are saved; the clock then restarts.
 */

#ifndef PORTAL_H
#define PORTAL_H

// Never returns (restarts the clock once WiFi is set up)
void runProvisioningPortal();

#endif // PORTAL_H
