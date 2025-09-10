
window.onload = () => { getStatus(); };

async function getStatus() {
	try {
		const response = await fetch("./properties/config.json");
		if (!response.ok) return;
		const config = await response.json();

		// Set the state
		if ('TimeZone' in config)
			document.getElementById('timezones').value = config.TimeZone;

		// Set quiet time start
		if ('QuietTimeStartHour' in config && 'QuietTimeStartMinute' in config) {
			const hour = padZero(config.QuietTimeStartHour);
			const minute = padZero(config.QuietTimeStartMinute);
			document.getElementById('quietTimeStart').value = `${hour}:${minute}`;
		}

		// Set quiet time end
		if ('QuietTimeEndHour' in config && 'QuietTimeEndMinute' in config) {
			const hour = padZero(config.QuietTimeEndHour);
			const minute = padZero(config.QuietTimeEndMinute);
			document.getElementById('quietTimeEnd').value = `${hour}:${minute}`;
		}

		// Set auto status updates checkbox
		if ('AutoStatusUpdates' in config) {
			document.getElementById('autoStatusUpdates').checked = !!config.AutoStatusUpdates;
		}
	} catch (e) {
		// Optionally handle error
	}
}

// Helper function to pad single digit numbers with leading zero
function padZero(num) {
	return num < 10 ? '0' + num : num;
}


async function sendPropertyChange(changes) {
	try {
		const response = await fetch("./properties/set?" + changes, { method: "POST" });
		return response.ok;
		// Result returned so that eventually I could revert the change in the UX
	} catch (e) {
		return false;
	}
}

// Global Changes

function timezoneOnChange(element) {
	sendPropertyChange("timezone=" + element.value);
}

// Time setting changes
function quietTimeStartPickerChanged(element) {
	if (!element.value) return;

	const [hours, minutes] = element.value.split(':');
	const hour = parseInt(hours, 10);
	const minute = parseInt(minutes, 10);

	// Validate input
	if (!isValidTime(hour, minute)) return;

	sendPropertyChange("quietTimeStartHour=" + hour + "&quietTimeStartMinute=" + minute);
}

function quietTimeEndPickerChanged(element) {
	if (!element.value) return;

	const [hours, minutes] = element.value.split(':');
	const hour = parseInt(hours, 10);
	const minute = parseInt(minutes, 10);

	// Validate input
	if (!isValidTime(hour, minute)) return;

	sendPropertyChange("quietTimeEndHour=" + hour + "&quietTimeEndMinute=" + minute);
}

// Helper function to validate time input
function isValidTime(hour, minute) {
	// Check if the values are actually numbers
	if (isNaN(hour) || isNaN(minute)) return false;

	// Check if the values are within valid ranges
	if (hour < 0 || hour > 23) return false;
	if (minute < 0 || minute > 59) return false;

	return true;
}

function autoStatusUpdatesChanged(element) {
	const enabled = element.checked ? 1 : 0;
	sendPropertyChange("autoStatusUpdates=" + enabled);
}