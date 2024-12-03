const canvas = document.getElementById('gameCanvas');
const context = canvas.getContext('2d');
const tileSize = 2; // Update the tileSize to match server
let gameMatrix = []; // Initialize the game matrix
let selectedCharacterType = "coin";
let selectedTroop = null;
var jsonStuff = '{"player":{"coins":0,"troops":0}}';

var obj = JSON.parse(jsonStuff);

var PlayerState = {
  coins: 0,
  cities: []
};

const fullscrBtn = document.getElementById('fullscreenBtn');

const ws = new WebSocket("ws://localhost:9001"); // For Development
// const ws = new WebSocket("ws://<server_ip>:9001"); // For Production

console.log("WS was created");

ws.onopen = function (event) {
  console.log("WebSocket connection opened.");
  //ws.send("Hello from client");
};

function setCharacterType(button) {
  selectedCharacterType = button.textContent.toLowerCase();
  if (selectedCharacterType !== 'select') {
    selectedTroop = null; // Deselect any selected troop if not in select mode
  }
  console.log("Character type selected: ", selectedCharacterType);
  updateList(selectedCharacterType);
}

ws.onmessage = function (event) {
  if (event.data === "ping") {
    console.log("Received keep-alive ping from server.");
    return;
  }
  handleServerMessage(event);
};

ws.onclose = function (event) {
  console.log("WebSocket connection closed.");
};

ws.onerror = function (event) {
  console.error("WebSocket error: ", event);
};

function goFullScreen() {
  var canvas = document.getElementById("gameCanvas");
  if (canvas.requestFullScreen)
    canvas.requestFullScreen();
  else if (canvas.webkitRequestFullScreen)
    canvas.webkitRequestFullScreen();
  else if (canvas.mozRequestFullScreen)
    canvas.mozRequestFullScreen();
}

function clearBoard() {
  changeGridPoint(1000, 1000, "white", null);
  window.location.reload();
}

canvas.addEventListener("click", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const col = Math.floor(x / tileSize);
  const row = Math.floor(y / tileSize);

  if (selectedCharacterType === 'select') {
    selectTroop(col, row);
  } else if (selectedCharacterType === 'move' && selectedTroop) {
    moveTroop(col, row);
  } else {
    changeGridPoint(col, row, "white", selectedCharacterType);
  }
  updateList();
  selectedTroop = "select";
});

function selectTroop(x, y) {
  sendMessageToServer(x, y, 'select');
  selectedCharacterType = 'move'; // Switch to move mode after selecting a troop
}

function moveTroop(x, y) {
  sendMessageToServer(x, y, 'move');
  selectedCharacterType = 'select'; // Switch back to select mode after moving the troop
}

function changeGridPoint(x, y, color) {
  const message = `${x},${y},${color},${selectedCharacterType}`;
  ws.send(message);
  console.log(`Sent message to change grid point (${x}, ${y}) to char ${selectedCharacterType} with color ${color}`);
}

function sendMessageToServer(x, y, type) {
  const message = `${x},${y},#000000,${type}`;
  console.log(`Sending message to server: ${message}`);
  ws.send(message);
}

function handleServerMessage(event) {
  console.log("Event Data (Raw): ", event.data);
  if (!event.data) {
    return;
  }
  obj = JSON.parse(event.data);
  console.log(obj);

  const updates = event.data.split(";");
  updates.forEach(update => {
    if (update.trim()) { // Ensure no empty segments
      const [x, y, color] = update.split(",");
      if (x !== undefined && y !== undefined && color !== undefined) {
        const xPos = parseInt(x);
        const yPos = parseInt(y);

        // Update the game matrix
        if (!gameMatrix[yPos]) {
          gameMatrix[yPos] = [];
        }
        gameMatrix[yPos][xPos] = color;

        // Redraw only the updated tile
        context.clearRect(xPos * tileSize, yPos * tileSize, tileSize, tileSize);
        context.fillStyle = color;
        context.fillRect(xPos * tileSize, yPos * tileSize, tileSize, tileSize);
      }
    }
  });
  updateList();
}

//Popup functionality
function showPopup() {
  const popup = document.getElementById('popup');
  popup.classList.remove('hidden');
}
function closePopup() {
  const popup = document.getElementById('popup');
  popup.classList.add('hidden');
}

const list = document.getElementById('dynamic-list');
//status functionality
function updateList(character) {
  if (obj.player == null) return;
  const items = [obj.player.coins, character];

  list.innerHTML = ''; // clear list

  const coinsItem = document.createElement('li');
  coinsItem.textContent = "coins: " + items[0];
  list.appendChild(coinsItem);

  const selectedItem = document.createElement('li')
  selectedItem.textContent = "CURRENT: " + items[1];
  list.appendChild(selectedItem);
  return;
}
updateList();

// Initial setup to clear the canvas and fill with an initial color if needed
function initializeGameMatrix() {
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = '#ffff'; // Initial background color
  context.fillRect(0, 0, canvas.width, canvas.height);
}

// Call this function to start the game
initializeGameMatrix();

