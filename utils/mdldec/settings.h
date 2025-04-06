#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

extern int globalsettings;

enum
{
	SETTINGS_LEGACYMOTION = BIT(0),
	SETTINGS_UVSHIFTING = BIT(1),
	SETTINGS_NOLOGS = BIT(2),
	SETTINGS_NOVALIDATION =  BIT(3),
	SETTINGS_SEPARATEANIMSFOLDER = BIT(4),
	SETTINGS_SEPARATETEXTURESFOLDER = BIT(5),
};

#endif // SETTINGS_H
