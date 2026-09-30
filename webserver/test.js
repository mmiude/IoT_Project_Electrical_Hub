const socket = new WebSocket("ws://localhost:8080/ws?hub=1")
socket.onmessage = (event) => console.log(event.data);