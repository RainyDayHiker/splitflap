window.onload = getNetworks;

async function getNetworks() {
	try {
		document.getElementById("network_list_status").innerHTML = "Scanning for networks...";

		const response = await fetch("networks");

		if (response.ok) {
			const responseText = await response.text();
			const networks = responseText.split(",");
			let list = document.getElementById("network_list");
			networks.forEach((networkName) => {
				let entry = document.createElement("tr");
				entry.innerHTML = "<td><div class=\"network_name\" onclick=\"update('" + networkName + "')\">" + networkName + "</div></td>";
				list.appendChild(entry);
			});
			document.getElementById("network_list_status").innerHTML = "";
		} else if (response.status === 204) {
			document.getElementById("network_list_status").innerHTML = "No networks found.";
		} else {
			document.getElementById("network_list_status").innerHTML = "An error occurred.";
		}
	} catch (error) {
		console.error('Error fetching networks:', error);
		document.getElementById("network_list_status").innerHTML = "An error occurred while scanning for networks.";
	}
}

function update(newValue) {
	document.getElementById('ssid').value = newValue;
	document.getElementById("pass").focus();
} 