const w = 1024;
const h = 576;
const msize = 6;
const State = {
	Beg: Symbol(0),
	Mid: Symbol(1),
	Min: Symbol(2),
	Max: Symbol(3)
}
const parambaseurl = '/axis-cgi/param.cgi?action=';
const paramgeturl = parambaseurl + 'list&group=opcuagaugereader.';
const paramseturl = parambaseurl + 'update&opcuagaugereader.';
var points = {};
points['maxX'] = 0;
points['maxY'] = 0;
points['midX'] = 0;
points['midY'] = 0;
points['minX'] = 0;
points['minY'] = 0;

function getPointText(param) {
	return param + ': (' + points[param + 'X'] + ', ' + points[param + 'Y'] + ')';
}

function drawMarker(X, Y) {
	ctx.beginPath();
	ctx.fillStyle = '#ffcc33';
	ctx.strokeStyle = 'white';
	ctx.arc(X, Y, msize, 0, 2 * Math.PI, false);
	ctx.fill();
	ctx.arc(X, Y, msize, 1, 2 * Math.PI, false);
	ctx.stroke();
	document.getElementById("values").textContent =
		getPointText('min') + ' ' + getPointText('mid') + ' ' + getPointText('max');
}

function drawMax() {
	drawMarker(points['maxX'], points['maxY']);
}

function drawMid() {
	drawMarker(points['midX'], points['midY']);
}

function drawMin() {
	drawMarker(points['minX'], points['minY']);
}

function drawDefaultPoints() {
	ctx.clearRect(0, 0, draw.width, draw.height);
	drawMax();
	drawMid();
	drawMin();
}

function getCurrentValue(param) {
	$.get(paramgeturl + param)
		.done(function(data) {
			points[param] = data.split('=')[1];
			console.log('Got ' + param + ' value ' + points[param]);
			drawDefaultPoints();
		})
		.fail(function(data) {
			alert('FAILED to set ' + param);
		});
}

function setParam(param, value) {
	points[param] = value;
	$.get(paramseturl + param + '=' + value)
		.done(function(data) {
			console.log('Set ' + param + ' to ' + value);
		})
		.fail(function(jqXHR, textStatus, errorThrown) {
			alert('FAILED to set ' + param);
		});
}

function initWithCurrentValues() {
	getCurrentValue('midX');
	getCurrentValue('midY');
	getCurrentValue('maxX');
	getCurrentValue('maxY');
	getCurrentValue('minX');
	getCurrentValue('minY');
}

function handleCoord(X, Y) {
	switch (curState) {
		case State.Mid:
			setParam('midX', X);
			setParam('midY', Y);
			ctx.clearRect(0, 0, draw.width, draw.height);
			drawMid();
			curState = State.Min;
			infoTxt.innerHTML = 'Please mark gauge minimum point';
			break;
		case State.Min:
			setParam('minX', X);
			setParam('minY', Y);
			drawMin();
			curState = State.Max;
			infoTxt.innerHTML = 'Please mark gauge maximum point';
			break;
		case State.Max:
			setParam('maxX', X);
			setParam('maxY', Y);
			drawMax();
		default:
			curState = State.Mid;
			infoTxt.innerHTML = 'Please mark gauge middle point (starts new calibration)';
			break;
	}
}

function handleMouseClick(event) {
	var offsets = document.getElementById('preview').getBoundingClientRect();
	var top = offsets.top;
	var left = offsets.left;
	var X = Math.round(event.clientX - left);
	var Y = Math.round(event.clientY - top);
	handleCoord(X, Y);
}

document.addEventListener('click', handleMouseClick);
var curState = State.Beg;
var preview = document.getElementById('preview');
var draw = document.getElementById('draw');
var infoTxt = document.getElementById('info');
preview.width = preview.style.width = draw.width = w;
preview.height = preview.style.height = draw.height = h;
preview.src = '/axis-cgi/mjpg/video.cgi?resolution=' + w + 'x' + h;
var ctx = draw.getContext('2d');
initWithCurrentValues();
handleCoord(0, 0);
