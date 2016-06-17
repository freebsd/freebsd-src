/*
 * $Id: hid-debug.h,v 1.3 2001/05/10 15:56:07 vojtech Exp $
 *
 *  (c) 1999 Andreas Gal		<gal@cs.uni-magdeburg.de>
 *  (c) 2000-2001 Vojtech Pavlik	<vojtech@suse.cz>
 *
 *  Some debug stuff for the HID parser.
 *
 *  Sponsored by SuSE
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@suse.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

struct hid_usage_entry {
	unsigned  page;
	unsigned  usage;
	char     *description;
};

static struct hid_usage_entry hid_usage_table[] = {
  {  1,      0, "GenericDesktop" },
    {0, 0x01, "Pointer"},
    {0, 0x02, "Mouse"},
    {0, 0x04, "Joystick"},
    {0, 0x05, "GamePad"},
    {0, 0x06, "Keyboard"},
    {0, 0x07, "Keypad"},
    {0, 0x08, "MultiAxis"},
      {0, 0x30, "X"},
      {0, 0x31, "Y"},
      {0, 0x32, "Z"},
      {0, 0x33, "Rx"},
      {0, 0x34, "Ry"},
      {0, 0x35, "Rz"},
      {0, 0x36, "Slider"},
      {0, 0x37, "Dial"},
      {0, 0x38, "Wheel"},
      {0, 0x39, "HatSwitch"},
    {0, 0x3a, "CountedBuffer"},
      {0, 0x3b, "ByteCount"},
      {0, 0x3c, "MotionWakeup"},
      {0, 0x3d, "Start"},
      {0, 0x3e, "Select"},
      {0, 0x40, "Vx"},
      {0, 0x41, "Vy"},
      {0, 0x42, "Vz"},
      {0, 0x43, "Vbrx"},
      {0, 0x44, "Vbry"},
      {0, 0x45, "Vbrz"},
      {0, 0x46, "Vno"},
    {0, 0x80, "SystemControl"}, 
      {0, 0x81, "SystemPowerDown"},
      {0, 0x82, "SystemSleep"},
      {0, 0x83, "SystemWakeUp"},
      {0, 0x84, "SystemContextMenu"},
      {0, 0x85, "SystemMainMenu"},
      {0, 0x86, "SystemAppMenu"},
      {0, 0x87, "SystemMenuHelp"},
      {0, 0x88, "SystemMenuExit"},
      {0, 0x89, "SystemMenuSelect"},
      {0, 0x8a, "SystemMenuRight"},
      {0, 0x8b, "SystemMenuLeft"},
      {0, 0x8c, "SystemMenuUp"},
      {0, 0x8d, "SystemMenuDown"},
    {0, 0x90, "D-padUp"},
    {0, 0x91, "D-padDown"},
    {0, 0x92, "D-padRight"},
    {0, 0x93, "D-padLeft"},
  {  7, 0, "Keyboard" },
  {  8, 0, "LED" },
  {  9, 0, "Button" },
  { 12, 0, "Hotkey" },
  { 13, 0, "Digitizers" },
    {0, 0x01, "Digitizer"},
    {0, 0x02, "Pen"},
    {0, 0x03, "LightPen"},
    {0, 0x04, "TouchScreen"},
    {0, 0x05, "TouchPad"},
    {0, 0x20, "Stylus"},
    {0, 0x21, "Puck"},
    {0, 0x22, "Finger"},
    {0, 0x30, "TipPressure"},
    {0, 0x31, "BarrelPressure"},
    {0, 0x32, "InRange"},
    {0, 0x33, "Touch"},
    {0, 0x34, "UnTouch"},
    {0, 0x35, "Tap"},
    {0, 0x39, "TabletFunctionKey"},
    {0, 0x3a, "ProgramChangeKey"},
    {0, 0x3c, "Invert"},
    {0, 0x42, "TipSwitch"},
    {0, 0x43, "SecondaryTipSwitch"},
    {0, 0x44, "BarrelSwitch"},
    {0, 0x45, "Eraser"},
    {0, 0x46, "TabletPick"},
  { 15, 0, "PhysicalInterfaceDevice" },
  { 0, 0, NULL }
};

static void resolv_usage_page(unsigned page) {
	struct hid_usage_entry *p;

	for (p = hid_usage_table; p->description; p++)
		if (p->page == page) {
			printk("%s", p->description);
			return;
		}
	printk("%04x", page);
}

static void resolv_usage(unsigned usage) {
	struct hid_usage_entry *p;

	resolv_usage_page(usage >> 16);
	printk(".");
	for (p = hid_usage_table; p->description; p++)
		if (p->page == (usage >> 16)) {
			for(++p; p->description && p->page == 0; p++)
				if (p->usage == (usage & 0xffff)) {
					printk("%s", p->description);
					return;
				}
			break;
		}
	printk("%04x", usage & 0xffff);
}

__inline__ static void tab(int n) {
	while (n--) printk(" ");
}

static void hid_dump_field(struct hid_field *field, int n) {
	int j;
	
	if (field->physical) {
		tab(n);
		printk("Physical(");
		resolv_usage(field->physical); printk(")\n");
	}
	if (field->logical) {
		tab(n);
		printk("Logical(");
		resolv_usage(field->logical); printk(")\n");
	}
	tab(n); printk("Usage(%d)\n", field->maxusage);
	for (j = 0; j < field->maxusage; j++) {
		tab(n+2);resolv_usage(field->usage[j].hid); printk("\n");
	}
	if (field->logical_minimum != field->logical_maximum) {
		tab(n); printk("Logical Minimum(%d)\n", field->logical_minimum);
		tab(n); printk("Logical Maximum(%d)\n", field->logical_maximum);
	}
	if (field->physical_minimum != field->physical_maximum) {
		tab(n); printk("Physical Minimum(%d)\n", field->physical_minimum);
		tab(n); printk("Physical Maximum(%d)\n", field->physical_maximum);
	}
	if (field->unit_exponent) {
		tab(n); printk("Unit Exponent(%d)\n", field->unit_exponent);
	}
	if (field->unit) {
		tab(n); printk("Unit(%u)\n", field->unit);
	}
	tab(n); printk("Report Size(%u)\n", field->report_size);
	tab(n); printk("Report Count(%u)\n", field->report_count);
	tab(n); printk("Report Offset(%u)\n", field->report_offset);

	tab(n); printk("Flags( ");
	j = field->flags;
	printk("%s", HID_MAIN_ITEM_CONSTANT & j ? "Constant " : "");
	printk("%s", HID_MAIN_ITEM_VARIABLE & j ? "Variable " : "Array ");
	printk("%s", HID_MAIN_ITEM_RELATIVE & j ? "Relative " : "Absolute ");
	printk("%s", HID_MAIN_ITEM_WRAP & j ? "Wrap " : "");
	printk("%s", HID_MAIN_ITEM_NONLINEAR & j ? "NonLinear " : "");
	printk("%s", HID_MAIN_ITEM_NO_PREFERRED & j ? "NoPrefferedState " : "");
	printk("%s", HID_MAIN_ITEM_NULL_STATE & j ? "NullState " : "");
	printk("%s", HID_MAIN_ITEM_VOLATILE & j ? "Volatile " : "");
	printk("%s", HID_MAIN_ITEM_BUFFERED_BYTE & j ? "BufferedByte " : "");
	printk(")\n");
}

static void hid_dump_device(struct hid_device *device) {
	struct hid_report_enum *report_enum;
	struct hid_report *report;
	struct list_head *list;
	unsigned i,k;
	static char *table[] = {"INPUT", "OUTPUT", "FEATURE"};
	
	for (i = 0; i < device->maxapplication; i++) {
		printk("Application(");
		resolv_usage(device->application[i]);
		printk(")\n");
	}

	for (i = 0; i < HID_REPORT_TYPES; i++) {
		report_enum = device->report_enum + i;
		list = report_enum->report_list.next;
		while (list != &report_enum->report_list) {
			report = (struct hid_report *) list;
			tab(2);
			printk("%s", table[i]);
			if (report->id)
				printk("(%d)", report->id);
			printk("[%s]", table[report->type]);
			printk("\n");
			for (k = 0; k < report->maxfield; k++) {
				tab(4);
				printk("Field(%d)\n", k);
				hid_dump_field(report->field[k], 6);
			}
			list = list->next;
		}
	}
}

static void hid_dump_input(struct hid_usage *usage, __s32 value) {
	printk("hid-debug: input ");
	resolv_usage(usage->hid);
	printk(" = %d\n", value);
}
