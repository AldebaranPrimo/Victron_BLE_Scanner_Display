#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include "config_manager.h"

void portalStart(const GatewayConfig& currentCfg);
void portalLoop();
bool portalSaved();
void portalStop();

#endif
