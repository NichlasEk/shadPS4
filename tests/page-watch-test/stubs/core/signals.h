// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace Core { struct Signals {
 static Signals* Instance(){static Signals value;return &value;}
 void RegisterAccessViolationHandler(bool(*)(void*,void*),unsigned) {}
}; }
