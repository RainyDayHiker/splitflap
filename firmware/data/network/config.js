window.onload = getNetworks;

function getNetworks() {
	document.getElementById("network_list_status").innerHTML = "Loading networks...";
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function () {
		if (this.readyState == 4) {
			if (this.status == 200) {
				const networks = this.responseText.split(",");
				let list = document.getElementById("network_list");
				networks.forEach((networkName) => {
					let entry = document.createElement("tr");
					entry.innerHTML = "<td><div class=\"network_name\" onclick=\"update('" + networkName + "')\">" + networkName + "</div></td>";
					list.appendChild(entry);
				});
				document.getElementById("network_list_status").innerHTML = "";
			} else if (this.status == 204) {
				document.getElementById("network_list_status").innerHTML = "No networks found.";
			} else {
				document.getElementById("network_list_status").innerHTML = "An error occurred.";
			}
		}
	};
	xhttp.open("GET", "networks", true);
	xhttp.send();
}

function update(newValue) {
	document.getElementById('ssid').value = newValue;
	document.getElementById("pass").focus();
} 