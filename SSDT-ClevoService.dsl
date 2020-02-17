/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20180427 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 */

DefinitionBlock ("", "SSDT", 1, "hack ", "CLEVO", 0x00000000)
{
    External (_SB_.AC__, DeviceObj)    // (from opcode)
    External (_SB_.AC__.ACFG, IntObj)    // (from opcode)
    External (_SB_.DCHU.ZEVT, MethodObj)    // 3 Arguments (from opcode)
    External (_SB_.PCI0, DeviceObj)    // (from opcode)
    External (_SB_.PCI0.LPCB, DeviceObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__, DeviceObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.AFLT, FieldUnitObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.AIRP, FieldUnitObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.BAT0, FieldUnitObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.ECOK, IntObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.OEM4, FieldUnitObj)    // (from opcode)
    External (_SB_.PCI0.LPCB.EC__.XQ50, MethodObj)    // 0 Arguments (from opcode)
    External (_SB_.PCI0.PEG0.PEGP._OFF, MethodObj)    // 0 Arguments (from opcode)
    External (_SB_.PCI0.LPCB.PS2K, DeviceObj)    // (from opcode)
    External (_SB_.WMI.WMBB, MethodObj)    // (from opcode)
    External (XWAK, MethodObj)    // 1 Arguments (from opcode)

    Scope (_SB.PCI0.LPCB.EC)
    {
        OperationRegion (EC82, EmbeddedControl, Zero, 0xFF)
        Field (EC82, ByteAcc, Lock, Preserve)
        {
            Offset (0xD0), 
            FCP0,   8, 
            FCP1,   8, 
            FGP0,   8, 
            FGP1,   8, 
            Offset (0xE0), 
            FGP2,   8, 
            FGP3,   8, 
        }
    }

    Method (_WAK, 1, Serialized)
    {
        If (Arg0 == 5) {Arg0 = 4}  //OSX also uses Arg0=5. Force 4 for compatibility...
        // Dual GPU OFF, remove it if not necessary
        \_SB.PCI0.PEG0.PEGP._OFF ()
        // After a sleep AFLT=1 means battery fail, we need to reset...
        // Remove it if not necessary
        Store (Zero, \_SB.PCI0.LPCB.EC.AFLT)
        //After a sleep sleep the battery is not present, we need to set...
        // Remove it if not necessary
        Store (One, \_SB.PCI0.LPCB.EC.BAT0)
        Notify (\_SB.CLV0, 0x83) //Set backlight according to settings
        Return (XWAK (Arg0))  //We're going to call original _WAK method...
    }

    Scope (_SB.PCI0.LPCB.EC)
    {
        Method (_Q50, 0, NotSerialized)  // _Qxx: EC Query
        {
            Store (OEM4, Local0)
            If (LEqual (Local0, 0x80))
            {
                Notify (CLV0, 0x80)
                Return (Zero)
            }

            If (LEqual (Local0, 0x81))
            {
                Notify (CLV0, 0x81)
                Return (Zero)
            }

            If (LEqual (Local0, 0x82))
            {
                Notify (CLV0, 0x82)
                Return (Zero)
            }

            If (LEqual (Local0, 0x9F))
            {
                Notify (CLV0, 0x9F)
                Return (Zero)
            }

            If (LEqual (Local0, 0xC9))
            {
                And (AIRP, 0xBF, AIRP) //Settiamo led Airplane per Shift-Lock
                Return (Zero)
            }

            If (LEqual (Local0, 0xCA))
            {
                Or (AIRP, 0x40, AIRP) //Restettiamo led Airplane per Shift-Lock
                Return (Zero)
            }

            XQ50 ()
            Return (Zero)
        }
    }

    Device (_SB.CLV0)
    {
        Name (_HID, EisaId ("PNP0C02"))  // _HID: Hardware ID
        Name (_CID, "MON0000")  // _CID: Compatible ID
        Name (KLVN, Zero)
        Name (TACH, Package (0x06)  //Aggiungiamo i nostri 3 ventilatori
        {
            "CPU Fan", 
            "VEN1", 
            "GPU Fan #1", 
            "VEN2", 
            "GPU Fan #2", 
            "VEN3"
        })
        Method (VEN1, 0, Serialized)
        {
            If (\_SB.PCI0.LPCB.EC.ECOK)
            {
                Store (B1B2 (\_SB.PCI0.LPCB.EC.FCP1, \_SB.PCI0.LPCB.EC.FCP0), Local0)
                If (LLessEqual (Local0, Zero))
                {
                    Return (Zero)
                }

                Divide (0x0020E6BC, Local0, , Local0)
                Return (Local0)
            }

            Return (Zero)
        }

        Method (VEN2, 0, Serialized)
        {
            If (\_SB.PCI0.LPCB.EC.ECOK)
            {
                Store (B1B2 (\_SB.PCI0.LPCB.EC.FGP1, \_SB.PCI0.LPCB.EC.FGP0), Local0)
                If (LLessEqual (Local0, Zero))
                {
                    Return (Zero)
                }

                Divide (0x0020E6BC, Local0, , Local0)
                Return (Local0)
            }

            Return (Zero)
        }

        Method (VEN3, 0, Serialized)
        {
            If (\_SB.PCI0.LPCB.EC.ECOK)
            {
                Store (B1B2 (\_SB.PCI0.LPCB.EC.FGP3, \_SB.PCI0.LPCB.EC.FGP2), Local0)
                If (LLessEqual (Local0, Zero))
                {
                    Return (Zero)
                }

                Divide (0x0020E6BC, Local0, , Local0)
                Return (Local0)
            }

            Return (Zero)
        }

        Method (CLVE, 3, Serialized)
        {
            If (CondRefOf( \_SB.DCHU.ZEVT ))   //Vers 1.0.1 17/02/2020
            {
                \_SB.DCHU.ZEVT (Arg0, Arg1, Arg2)
            } elseIf (CondRefOf( \_SB.WMI.WMBB ))  //Added support for WMBB method
            {
                \_SB.WMI.WMBB (Arg0, Arg1, Arg2)
            }
        }

        Method (_INI, 0, NotSerialized)  // _INI: Initialize
        {
            // DGPU OFF, Remove it or update path as necessary...
            If (CondRefOf (\_SB.PCI0.PEG0.PG00._OFF))
            {
                \_SB.PCI0.PEG0.PEGP._OFF ()
            }

            If (CondRefOf (\_SB.PCI0.LPCB.EC.AIRP))
            {
                And (\_SB.PCI0.LPCB.EC.AIRP, 0xBF, \_SB.PCI0.LPCB.EC.AIRP)
            }
        }
    }

    Method (B1B2, 2, NotSerialized)
    {
        Return (Or (Arg0, ShiftLeft (Arg1, 0x08)))
    }
}

