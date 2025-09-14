const grid = document.getElementById('splitflap-grid');
let state = null;
// Cache of module DOM nodes by module index
const moduleNodes = new Map();
let lastLayout = { cols: null, rows: null };

// Polling control
const POLL_INTERVAL_MS = 2000; // fetch every 2s while active
const POLL_ACTIVE_DURATION_MS = 2 * 60 * 1000; // stop after 2 minutes
const pollStatusEl = document.getElementById('poll-status');
const toggleBtn = document.getElementById('toggle-poll');
let pollIntervalId = null;
let pollStartTime = null;

// Helper: wrap an Instruction font character with any needed centering class
function renderInstructionChar(ch) {
	let specialClass = '';
	switch (ch) {
		case ':': specialClass = 'flap-center-colon'; break;
		case '!': specialClass = 'flap-center-exclam'; break;
		case '.': specialClass = 'flap-center-dot'; break;
		case ',': specialClass = 'flap-center-comma'; break;
		case "'": specialClass = 'flap-center-quote'; break;
	}
	return `<span class="flap-instruction${specialClass ? ' ' + specialClass : ''}">${ch}</span>`;
}

function getStateName(code) {
	switch (code) {
		case 0: return 'NORMAL';
		case 1: return 'LOOK_FOR_HOME';
		case 2: return 'SENSOR_ERROR';
		case 3: return 'PANIC';
		case 4: return 'DISABLED';
		default: return 'UNKNOWN';
	}
}

function fetchState() {
	fetch('splitflap/state.json')
		.then(r => r.json())
		.then(data => {
			const previous = state;
			state = data;
			renderOrUpdate(previous, state);
		})
		.catch(() => {
			grid.innerHTML = '<div style="text-align:center;color:#c00;">Error loading state</div>';
			moduleNodes.clear();
		});
}

function renderOrUpdate(prev, curr) {
	if (!curr) return;
	const cols = curr.display_columns;
	const rows = curr.display_rows;

	// Detect layout change
	const layoutChanged = (lastLayout.cols !== cols) || (lastLayout.rows !== rows);
	if (layoutChanged) {
		grid.style.gridTemplateColumns = `repeat(${cols}, 140px)`;
		grid.style.gridTemplateRows = `repeat(${rows}, 220px)`;
		grid.innerHTML = '';
		moduleNodes.clear();
		lastLayout = { cols, rows };
	}

	const seen = new Set();
	for (const flap of curr.modules) {
		const key = flap.index; // assume stable unique
		seen.add(key);
		let node = moduleNodes.get(key);
		if (!node) {
			node = createModuleNode(flap, curr);
			moduleNodes.set(key, node);
			grid.appendChild(node.container);
		} else {
			updateModuleNode(node, flap, curr, prev);
		}
	}

	// Remove nodes no longer present
	for (const [k, v] of moduleNodes.entries()) {
		if (!seen.has(k)) {
			v.container.remove();
			moduleNodes.delete(k);
		}
	}
}

function createModuleNode(flap, stateObj) {
	const container = document.createElement('div');
	container.className = 'flap-box';
	container.style.gridColumn = flap.col + 1;
	container.style.gridRow = flap.row + 1;
	container.setAttribute('data-row', flap.row);
	container.setAttribute('data-col', flap.col);

	const charWrapper = document.createElement('div');
	charWrapper.className = 'flap-char-wrapper';
	container.appendChild(charWrapper);

	const offsetControls = document.createElement('div');
	offsetControls.className = 'offset-controls';
	offsetControls.innerHTML = `
		<button class="offset-btn" onclick="changeOffset(${flap.index}, -5)">&lt;&lt;</button>
		<button class="offset-btn" onclick="changeOffset(${flap.index}, -1)">&lt;</button>
		<span class="offset-value">${flap.offset}</span>
		<button class="offset-btn" onclick="changeOffset(${flap.index}, 1)">&gt;</button>
		<button class="offset-btn" onclick="changeOffset(${flap.index}, 5)">&gt;&gt;</button>
	`;
	container.appendChild(offsetControls);

	const iconDiv = document.createElement('div');
	iconDiv.className = 'icon';
	container.appendChild(iconDiv);

	// Initial fill
	fillCharAndIcon(charWrapper, iconDiv, flap, stateObj);

	return { container, charWrapper, iconDiv, lastFlap: { ...flap } };
}

function updateModuleNode(node, flap, curr, prev) {
	// Position changes
	if (node.lastFlap.row !== flap.row || node.lastFlap.col !== flap.col) {
		node.container.style.gridColumn = flap.col + 1;
		node.container.style.gridRow = flap.row + 1;
		node.container.setAttribute('data-row', flap.row);
		node.container.setAttribute('data-col', flap.col);
	}
	// Offset value change
	if (node.lastFlap.offset !== flap.offset) {
		const span = node.container.querySelector('.offset-value');
		if (span) span.textContent = flap.offset;
	}
	// Content/state change check
	if (
		node.lastFlap.flap_index !== flap.flap_index ||
		node.lastFlap.state !== flap.state ||
		node.lastFlap.moving !== flap.moving ||
		(curr && prev && curr.flaps[flap.flap_index] !== prev.flaps[node.lastFlap.flap_index])
	) {
		fillCharAndIcon(node.charWrapper, node.iconDiv, flap, curr);
	}
	node.lastFlap = { ...flap };
}

function fillCharAndIcon(charWrapper, iconDiv, flap, stateObj) {
	const rows = stateObj.display_rows;
	const cols = stateObj.display_columns;
	const isLastFlap = (flap.row === rows - 1) && (flap.col === cols - 1);
	const flapCharRaw = stateObj.flaps[flap.flap_index] || '?';

	let html = '';
	if (flap.state === 0) { // NORMAL
		if (isLastFlap) {
			switch (flapCharRaw) {
				case 'a': html = `<span class="flap-emoji">&#x1F525;</span>`; break;
				case 'b': html = `<span class="flap-emoji">&#x2601;</span>`; break;
				case 'c': html = `<span class="flap-emoji">&#x1F327;</span>`; break;
				case 'd': html = `<span class="flap-emoji">&#x2744;</span>`; break;
				case 'e': html = `<span class="flap-emoji">&#x1F32C;</span>`; break;
				case 'f': html = `<span class="flap-emoji">&#x26C5;</span>`; break;
				default: html = renderInstructionChar(flapCharRaw); break;
			}
		} else {
			switch (flapCharRaw) {
				case 'a': html = `<div class="flap-color-box flap-green"></div>`; break;
				case 'b': html = `<div class="flap-color-box flap-red"></div>`; break;
				case 'c': html = `<span class="flap-emoji">&#x1F603;</span>`; break;
				case 'd': html = `<span class="flap-emoji">&#x2639;</span>`; break;
				case 'e': html = `<span class="flap-emoji">&#x1F525;</span>`; break;
				case 'f': html = `<span class="flap-emoji">&#x1F90D;</span>`; break;
				default: html = renderInstructionChar(flapCharRaw); break;
			}
		}
	} else {
		html = `<span>&nbsp;</span>`;
	}
	if (charWrapper.innerHTML !== html) {
		charWrapper.innerHTML = html;
	}

	const iconHtml = getFlapIcon(flap);
	if (iconDiv.innerHTML !== iconHtml) {
		iconDiv.innerHTML = iconHtml;
	}
	let tooltip = getStateName(flap.state);
	if (flap.state === 0 && flap.moving) tooltip += ' (moving)';
	if (iconDiv.title !== tooltip) iconDiv.title = tooltip;
}

function getFlapIcon(flap) {
	// State enum (from firmware):
	// 0 NORMAL, 1 LOOK_FOR_HOME, 2 SENSOR_ERROR, 3 PANIC, 4 STATE_DISABLED
	switch (flap.state) {
		case 0: // NORMAL
			// If moving
			if (flap.moving) {
				return `<svg viewBox="0 0 20 20" fill="#3498db"><circle cx="10" cy="10" r="9"/><path d="M8 6l5 4-5 4V6z" fill="#fff"/></svg>`;
			}
			return '';
		case 1: // LOOK_FOR_HOME
			return `<svg viewBox="0 0 20 20" fill="#f1c40f"><circle cx="10" cy="10" r="9"/><path d="M10 5l4 4-1.4 1.4L11 8.8V15H9V8.8L7.4 10.4 6 9z" fill="#fff"/></svg>`;
		case 2: // SENSOR_ERROR
			return `<svg viewBox="0 0 20 20" fill="#e74c3c"><circle cx="10" cy="10" r="9"/><path d="M9 5h2v6H9zm0 8h2v2H9z" fill="#fff"/></svg>`;
		case 3: // PANIC
			return `<svg viewBox="0 0 20 20" fill="#8e44ad"><circle cx="10" cy="10" r="9"/><path d="M5 6l5 8 5-8H5z" fill="#fff"/></svg>`;
		case 4: // STATE_DISABLED
			return `<svg viewBox="0 0 20 20" fill="#7f8c8d"><circle cx="10" cy="10" r="9"/><path d="M6 6h8v8H6z" fill="#fff"/></svg>`;
		default:
			return '';
	}
}

window.changeOffset = function (index, delta) {
	// TODO: Implement backend call to change offset
	alert(`Change offset for module ${index} by ${delta}`);
};

function stopPolling(timeout = false) {
	if (pollIntervalId !== null) {
		clearInterval(pollIntervalId);
		pollIntervalId = null;
	}
	if (timeout) {
		pollStatusEl.textContent = 'Paused after 2 minutes';
	} else {
		pollStatusEl.textContent = 'Stopped';
	}
	toggleBtn.textContent = 'Resume Polling';
}

function startPolling() {
	if (pollIntervalId !== null) {
		clearInterval(pollIntervalId);
	}
	pollStatusEl.textContent = 'Polling...';
	toggleBtn.textContent = 'Stop Polling';
	pollStartTime = Date.now();
	fetchState();
	pollIntervalId = setInterval(() => {
		const elapsed = Date.now() - pollStartTime;
		if (elapsed >= POLL_ACTIVE_DURATION_MS) {
			stopPolling(true);
			return;
		}
		fetchState();
	}, POLL_INTERVAL_MS);
}

toggleBtn.addEventListener('click', () => {
	if (pollIntervalId === null) {
		startPolling();
	} else {
		stopPolling(false);
	}
});

// Kick off polling lifecycle
startPolling();
