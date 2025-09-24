let flapState = [];
let offsets = [];
let modulesMeta = [];// stores full module objects including row/col
let displayColumns = 0; // number of columns for grid layout
let offsetMin = 0;
let offsetMaxExclusive = null; // steps_per_revolution

function fetchState() {
	fetch('/splitflap/state.json').then(r => r.json()).then(data => {
		if (data && Array.isArray(data.flaps)) {
			FLAP_CHARS = data.flaps;
		}
		modulesMeta = data.modules.map(m => ({
			index: m.index,
			flap_index: m.flap_index,
			offset: m.offset,
			row: m.row,
			col: m.col
		}));
		displayColumns = data.display_columns || 0;
		flapState = modulesMeta.map(m => m.flap_index);
		offsets = modulesMeta.map(m => m.offset);
		offsetMin = (typeof data.offset_min === 'number') ? data.offset_min : 0;
		offsetMaxExclusive = (typeof data.offset_max_exclusive === 'number') ? data.offset_max_exclusive : (data.steps_per_revolution || null);
		numFlaps = data.num_flaps || (FLAP_CHARS.length > 0 ? FLAP_CHARS.length : 52);
		updateRangeInfo();
		renderGrid();
	});
}

function updateRangeInfo() {
	const el = document.getElementById('rangeInfo');
	if (!el) return;
	if (offsetMaxExclusive != null) {
		el.textContent = `Offset range: ${offsetMin} to ${offsetMaxExclusive - 1} (${Math.round(offsetMaxExclusive / numFlaps)} per flap)`;
	} else {
		el.textContent = '';
	}
}

function renderGrid() {
	const grid = document.getElementById('grid');
	grid.innerHTML = '';
	if (displayColumns > 0) {
		grid.style.gridTemplateColumns = `repeat(${displayColumns}, minmax(60px, 1fr))`;
	}
	// Sort modules by row, then col to render in layout order
	const sorted = [...modulesMeta].sort((a, b) => (a.row - b.row) || (a.col - b.col));
	sorted.forEach(mod => {
		const i = mod.index; // original module index used for offsets array mapping
		const cell = document.createElement('div');
		cell.className = 'grid-cell';
		cell.style.gridColumn = (mod.col + 1);
		cell.style.gridRow = (mod.row + 1);

		// Editable number input for offset value
		const input = document.createElement('input');
		input.type = 'number';
		input.className = 'offset-input';
		input.value = offsets[i];
		input.setAttribute('data-idx', i);
		if (offsetMaxExclusive != null) {
			input.min = offsetMin;
			input.max = offsetMaxExclusive - 1; // inclusive max for HTML attribute
		}
		input.onchange = () => {
			let val = parseInt(input.value);
			if (isNaN(val)) return;
			if (offsetMaxExclusive != null) {
				if (val < offsetMin) val = offsetMin;
				if (val >= offsetMaxExclusive) val = offsetMaxExclusive - 1;
				input.value = val;
			}
			offsets[i] = val;
		};
		cell.appendChild(input);

		// Add "Pick Flap" button
		const pickBtn = document.createElement('button');
		pickBtn.textContent = 'Flap';
		pickBtn.className = 'pick-flap-btn';
		pickBtn.onclick = () => showFlapPicker(i);
		cell.appendChild(pickBtn);

		grid.appendChild(cell);
	});
}

// --- Flap picker logic ---
// Flap characters will be set from state.json

let FLAP_CHARS = [];
let pickerModuleIndex = null;

function showFlapPicker(idx) {
	const overlay = document.getElementById('char-picker-overlay');
	const grid = document.getElementById('char-picker-grid');
	const cancelBtn = document.getElementById('char-picker-cancel');
	pickerModuleIndex = idx;
	grid.innerHTML = '';
	// Current flap index for this module
	const currentFlapIdx = flapState[idx];
	FLAP_CHARS.forEach((ch, flapIdx) => {
		const btn = document.createElement('div');
		btn.textContent = ch;
		btn.className = 'char-option';
		if (flapIdx === currentFlapIdx) btn.classList.add('current');
		btn.onclick = (e) => {
			e.stopPropagation();
			closeFlapPicker();
			handleFlapPicked(idx, flapIdx);
		};
		grid.appendChild(btn);
	});
	overlay.style.display = 'flex';
	cancelBtn.onclick = closeFlapPicker;
	overlay.onclick = (e) => {
		if (e.target === overlay) closeFlapPicker();
	};
}

function closeFlapPicker() {
	const overlay = document.getElementById('char-picker-overlay');
	overlay.style.display = 'none';
	pickerModuleIndex = null;
}

function handleFlapPicked(idx, pickedFlapIdx) {
	// Determine last set flap for this module (A or 1)
	let lastFlapIdx = (flapState[idx] >= 0 && flapState[idx] < FLAP_CHARS.length) ? flapState[idx] : FLAP_CHARS.indexOf('A');
	if (lastFlapIdx === -1) lastFlapIdx = FLAP_CHARS.indexOf('A');
	if (lastFlapIdx === -1) lastFlapIdx = 0;

	const stepsPerFlap = offsetMaxExclusive && numFlaps ? Math.round(offsetMaxExclusive / numFlaps) : 1;
	const delta = (pickedFlapIdx - lastFlapIdx) * stepsPerFlap * -1; // Inverted direction
	let newOffset = offsets[idx] + delta;

	// Modulo-style rollover for offset
	if (offsetMaxExclusive != null) {
		const range = offsetMaxExclusive - offsetMin;
		newOffset = ((newOffset - offsetMin) % range + range) % range + offsetMin;
	}
	offsets[idx] = newOffset;
	renderGrid();
}

function changeOffset(idx, delta) {
	offsets[idx] += delta;
	if (offsetMaxExclusive != null) {
		if (offsets[idx] < offsetMin) offsets[idx] = offsetMin;
		if (offsets[idx] >= offsetMaxExclusive) offsets[idx] = offsetMaxExclusive - 1;
	}
}

document.getElementById('apply').onclick = function () {
	// Final validation before submit
	if (offsetMaxExclusive != null) {
		for (let i = 0; i < offsets.length; i++) {
			if (offsets[i] < offsetMin || offsets[i] >= offsetMaxExclusive) {
				alert(`Offset ${i} out of range (${offsets[i]}). Allowed: ${offsetMin} - ${offsetMaxExclusive - 1}`);
				return;
			}
		}
	}
	const csv = offsets.join(',');
	const body = 'offsets=' + encodeURIComponent(csv);
	fetch('/splitflap/set_home_offsets', {
		method: 'POST',
		headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
		body
	}).then(r => r.json ? r.json().catch(() => ({})) : Promise.resolve({}))
		.then(() => setTimeout(fetchState, 2000));
};

document.getElementById('testA').onclick = function () {
	const str = 'A'.repeat(offsets.length);
	setFlapState(str);
};
document.getElementById('test1').onclick = function () {
	const str = '1'.repeat(offsets.length);
	setFlapState(str);
};

function setFlapState(str) {
	const body = 's=' + encodeURIComponent(str);
	fetch('/splitflap/set_flaps', {
		method: 'POST',
		headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
		body
	}).then(r => r.json ? r.json().catch(() => ({})) : Promise.resolve({}))
		.then(() => setTimeout(fetchState, 2000));
}

window.onload = fetchState;

// Copy offsets to clipboard in C array format
document.getElementById('copyOffsets').onclick = function () {
	try {
		const lines = offsets.map(v => `\t${v},`);
		const content = [
			'const uint16_t default_offsets[NUM_MODULES] = {',
			...lines,
			'};',
			''
		].join('\n');
		const write = (text) => navigator.clipboard && navigator.clipboard.writeText ? navigator.clipboard.writeText(text) : Promise.reject();
		write(content).then(() => {
			console.log('Offsets copied to clipboard');
		}).catch(() => {
			// Fallback: create temporary textarea
			const ta = document.createElement('textarea');
			ta.value = content;
			document.body.appendChild(ta);
			ta.select();
			try { document.execCommand('copy'); } catch (e) { console.warn('Copy failed', e); }
			document.body.removeChild(ta);
		});
	} catch (e) {
		console.error('Failed to build offsets clipboard content', e);
	}
};

document.getElementById('saveOffsets').onclick = function () {
	fetch('/splitflap/save_offsets', { method: 'POST' })
		.then(() => alert('Offsets saved!'))
		.catch(() => alert('Failed to save offsets.'));
};