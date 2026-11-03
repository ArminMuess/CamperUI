#pragma once

// Schedule a documented RGB DMA restart after Flash/NVS writes finish.
// Called by UI settings handlers; the LVGL loop performs the operation.
void display_request_resync();
