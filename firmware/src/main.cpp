// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Verdrahtet die Module, den Befehls-Router und die Serial-Konsole.
// ============================================================================
#include <Arduino.h>

#include "config.h"
#include "storage/StorageService.h"
#include "core/CommandRouter.h"
#include "modules/display/DisplayModule.h"
#include "modules/input/InputModule.h"
#include "modules/ir/IrModule.h"
#include "modules/nfc/NfcModule.h"
#include "console/SerialConsole.h"

DisplayModule display;
InputModule input;
NfcModule nfc;
IrModule ir;

Module* modules[] = {&display, &input, &nfc, &ir};
static const size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

CommandRouter router;

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  Storage.begin();
  for (Module* module : modules) module->begin();

  router.begin(modules, MODULE_COUNT);
  display.setCommandRouter(&router);
  Console.begin(&router);

  Serial.println("[OrcaOne] Bereit. 'help' fuer Befehle.");
}

void loop() {
  for (Module* module : modules) module->update();

  Console.update();
  InputEvent event = input.poll();
  while (event != InputEvent::None) {
    display.handleInput(event);
    event = input.poll();
  }

  delay(1);
}
