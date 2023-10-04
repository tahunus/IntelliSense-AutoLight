const temperatures = [2700, 3400, 4000, 5200, 6500];
const defaultTemps = [0, 1, 1, 2, 2, 4, 4, 4, 3, 3, 2, 2, 1, 1, 0];
let selectedTemps = [...defaultTemps];

// Temperature to RGB mapping
const temperatureColors = {
  2700: 'rgb(250, 180, 50)',
  3400: 'rgb(250, 220, 132)',
  4000: 'rgb(255, 249, 220)',
  5200: 'rgb(220, 248, 255)',
  6500: 'rgb(210, 252, 255)'
};

function updateBackgroundColor(dropdown) {
  let selectedTemperature = temperatures[parseInt(dropdown.value)];
  let selectedColor = temperatureColors[selectedTemperature];

  // Update the td background color
  dropdown.parentElement.style.backgroundColor = selectedColor;
  // Update the dropdown background color
  dropdown.style.backgroundColor = selectedColor; 
  let thElement = document.querySelector(`#temperatureTable th:nth-child(${Array.from(dropdown.parentElement.parentElement.children).indexOf(dropdown.parentElement) + 1})`);
  // Update the background color of the th element
  if (thElement) {
    thElement.style.backgroundColor = selectedColor;
  }
}

window.addEventListener('load', function() {
  var inactivityTimer;
  function resetInactivityTimer() {
    clearTimeout(inactivityTimer);
    inactivityTimer = setTimeout(showReloadMessage, 5 * 60 * 1000); // 5 minutes in milliseconds
  }
  function showReloadMessage() {
    alert('You have been inactive for 5 minutes.\nPlease reload the page to ensure updated data.');
  }

  // facesTable (parent) to control visibility and facesTableBody (child) for data
  var lightsTableBody = document.getElementById('lightsTableBody');
  var facesTableBody = document.getElementById('facesTableBody');
  var startupMessage = document.getElementById('startupMessage');
  var statusMessage = document.getElementById('statusMessage');
  //must contain the same elements that in C++ file
  var options = ["XX", "Romance", "Sunset", "XX", "XX", "Cozy", "Forest", "XX", "XX", "XX", "Warm_white", 
                 "Daylight", "Cool_white", "Night_light", "Focus", "Relax", "XX", "TV_Time", "XX", "XX", "XX", 
                 "XX", "XX", "XX", "XX", "XX", "XX", "XX", "Candlelight", "Golden_white"];
  startupMessage.innerHTML = '<span class="spinner"></span>Please wait, discovering lights in the network ...';
  startupMessage.style.display = 'block';
  var temperatureTitle = document.querySelector('.temperatureTitle');
  temperatureTitle.style.display = 'none';
  
  // first table = discovered lights
  var xhrDiscover = new XMLHttpRequest();
  xhrDiscover.open('GET', '/discoverLights', true); 
  xhrDiscover.onreadystatechange = function() {
    if (xhrDiscover.readyState === 4 && xhrDiscover.status === 200) {
      var response = JSON.parse(xhrDiscover.responseText);
      var tableData = response.devices; // = array with devices data
      lightsTableBody.innerHTML = '';
      for (var i = 0; i < tableData.length; i++) {
        var row = document.createElement('tr');
        var ipAddressCell = document.createElement('td');
        var MACaddressCell = document.createElement('td');
        var RSSICell = document.createElement('td');
        var stateCell = document.createElement('td');
        var checkboxCell = document.createElement('td');
        var rowData = tableData[i];
        // Set values for each cell
        ipAddressCell.textContent = rowData.ipAddress;
        MACaddressCell.textContent = rowData.MACAddress;
        RSSICell.textContent = rowData.rssi;
        stateCell.textContent = rowData.state ? 'On' : 'Off'; //convert boolean to text
        checkboxCell.innerHTML = `<input type="checkbox" ${rowData.enrolled ? 'checked' : ''}>`;
        // Append cells to the row
        row.appendChild(ipAddressCell);
        row.appendChild(MACaddressCell);
        row.appendChild(RSSICell);
        row.appendChild(stateCell);
        row.appendChild(checkboxCell);
        lightsTableBody.appendChild(row);
      }
      startupMessage.style.display = 'none';
      document.getElementById('lightsTable').style.display = "table"; // make the table visible

      doTemperatureTable();
      doSecondTable(); // to ensure second table starts only after first table is finished (& avoid async issues)
    }
  };
  xhrDiscover.send();

  function doTemperatureTable() {
    var xhrGetTODTemps = new XMLHttpRequest();
    xhrGetTODTemps.open('GET', '/getTODtemps', true);
    xhrGetTODTemps.onreadystatechange = function() {
      if (xhrGetTODTemps.readyState !== 4) return;
//>> The onreadystatechange event is fired multiple times throughout the lifecycle of the XMLHttpRequest, 
//>> representing different states of the request. Possible values 0=unsent, its constructed not yet initialized
//>> 1=Open, request is set up not yet sent, 2=Headers received, 3=Loading, partial data conatined, 
//>> 4=done, full response received & connection closed  
//>> so checking for status before readyState entails it runs multiple times
      if (xhrGetTODTemps.status >= 200 && xhrGetTODTemps.status < 400) {
        selectedTemps = JSON.parse(xhrGetTODTemps.responseText).temperatureData;
  
        // Populate Time of Day headers
        const headerRow = document.querySelector("#temperatureTable thead tr");
        for (let i = 6; i <= 20; i++) {
          const th = document.createElement("th");
          th.innerText = i;
          headerRow.appendChild(th);
        }
        // Populate Temperature dropdowns
        const tempRow = document.querySelector("#temperatureTable tbody tr");
        for (let i = 0; i < 15; i++) {
          const td = document.createElement("td");
          const select = document.createElement("select");
          select.className = "temperature-dropdown";
          td.appendChild(select);
          tempRow.appendChild(td);
        }
  
        const dropdowns = document.querySelectorAll(".temperature-dropdown");
        // populate each dropdown
        dropdowns.forEach((dropdown, i) => {
          temperatures.forEach((temp) => {
            const option = document.createElement("option");
            option.value = temperatures.indexOf(temp);
            option.innerText = temp;
            dropdown.appendChild(option);
          });
          dropdown.value = selectedTemps[i];
          updateBackgroundColor(dropdown);
          dropdown.addEventListener("change", function() {
            selectedTemps[i] = parseInt(this.value);
            updateBackgroundColor(dropdown);
          });
        });
  
        temperatureTitle.style.display = 'block';
        document.getElementById('temperatureTable').style.display = "table"; // make the table visible
        document.getElementById('resetBtn').style.display = "block";
      }else
        console.error('TOD Temps Error status code:', xhrGetTODTemps.status, 'response:', xhrGetTODTemps.responseText);
    };
    xhrGetTODTemps.send();
  }

  //second table = face IDs
  function doSecondTable() {
    startupMessage.textContent = 'Please wait, fetching Face ID data ...';
    startupMessage.style.display = 'block';
    var xhrGetFaceIDs = new XMLHttpRequest();
    xhrGetFaceIDs.open('GET', '/getFaceIDs', true); 
    xhrGetFaceIDs.onload = function() {
      if (xhrGetFaceIDs.status >=200 && xhrGetFaceIDs.status < 400) {
        var response = JSON.parse(xhrGetFaceIDs.responseText);
        var faceIDs = response.faceIDs; // 'faceIDs' is a named identifier in C++ code too
        //console.log('Face Data:', response); // log table data
        facesTableBody.innerHTML = ''; 
        for (var i = 0; i < faceIDs.length; i++) {
          var row = document.createElement('tr');
          var idCell = document.createElement('td');
          var labelCell = document.createElement('td');   
          idCell.textContent = faceIDs[i].ID;
          labelCell.textContent = faceIDs[i].label;
          if (faceIDs[i].label !== "NOT ENROLLED") { //don't edit name is no gace has been enrolled
            labelCell.contentEditable = "true";   
            labelCell.addEventListener('input', function() {
              if (this.textContent.length > 15) {
                this.textContent = this.textContent.substring(0, 15);
              }
            });
          }
          row.appendChild(idCell);
          row.appendChild(labelCell);
          // create dropdown list cells
          for (var j = 0; j < 3; j++) {
            var dropdownCell = document.createElement('td');
            var dropdown = document.createElement('select');
            var k = 1;
            options.forEach(function(option) {
              if (option !== "XX") {
                var optionElement = document.createElement('option');
                optionElement.textContent = option;
                optionElement.value = option;
                if (k == faceIDs[i].scenes[j]) optionElement.selected = true;
                dropdown.appendChild(optionElement);
              }
              k++;
            });
            dropdownCell.appendChild(dropdown);
            row.appendChild(dropdownCell);
          }
          facesTableBody.appendChild(row);
        }
        startupMessage.style.display = 'none';
        document.getElementById('facesTable').style.display = "table"; // make the table visible
        document.getElementById('confirmBtn').style.display = "block";
        var lightsTitle = document.querySelector('.lightsTitle');
        lightsTitle.style.display = 'block';
        var facesTitle = document.querySelector('.facesTitle');
        facesTitle.style.display = 'block';
      }else
        console.error('Faces Error status code:', xhrGetFaceIDs.status, 'response:', xhrGetFaceIDs.responseText);
    };
    xhrGetFaceIDs.send();
    resetInactivityTimer();
  } // end of doSecondTable function

  document.getElementById('confirmBtn').addEventListener('click', function() {
    statusMessage.style.backgroundColor = "rgb(255, 119, 0)";
    statusMessage.style.color = "white";    
    statusMessage.innerHTML = '<span class="spinner"></span>Please wait, saving data ...';
    statusMessage.style.display = 'block';
   // first table> enrolled lights
    var lightsTableData = [];
    var lightRows = document.querySelectorAll('#lightsTableBody tr');
    for (var i = 0; i < lightRows.length; i++) {
      var lightRow = lightRows[i];
      var lightRowData = {
        'IP Address': lightRow.cells[0].textContent,
        'state': lightRow.cells[3].textContent === 'On' ? true : false,
        'Selected': lightRow.cells[4].querySelector('input[type="checkbox"]').checked
      };
      lightsTableData.push(lightRowData);
    }
    // Second table: Read and save face IDs data
    var faceTableData = [];
    var faceRows = document.querySelectorAll('#facesTableBody tr');
    for (var i = 0; i < faceRows.length; i++) {
      var faceRow = faceRows[i];
      var faceRowData = {
        'ID': faceRow.cells[0].textContent,
        'Label': faceRow.cells[1].textContent,
        'Scenes': [ //list in JS is 0-based but in C++ is an ENUM 1-based for WiZ Pilot commands
          options.indexOf(faceRow.cells[2].querySelector('select').value) + 1,
          options.indexOf(faceRow.cells[3].querySelector('select').value) + 1,
          options.indexOf(faceRow.cells[4].querySelector('select').value) + 1
        ]
      };
      faceTableData.push(faceRowData);
    }

    // Combine the data from both tables into one JSON object
    var combinedData = {
      'lightsData': lightsTableData,
      'faceData': faceTableData,
      'temperatureData': selectedTemps
    };
    var jsonData = JSON.stringify(combinedData);
    var xhrConfirm = new XMLHttpRequest();
    xhrConfirm.open('GET', '/saveData?data=' + encodeURIComponent(jsonData), true);
    xhrConfirm.onload = function() {
      if (xhrConfirm.status >= 200 && xhrConfirm.status < 400) {
        statusMessage.textContent = xhrConfirm.responseText || 'No response received';
        document.getElementById("statusMessage").style.backgroundColor = "transparent";
        document.getElementById("statusMessage").style.color = "black"; 
        statusMessage.style.display = 'block';
      } else {
        console.error('Confirm Error status code:', xhrConfirm.status, 'response:', xhrConfirm.responseText);
      }
    };
    xhrConfirm.send();
    resetInactivityTimer();
  }); // end of confirmBtn
 

});  // end of event window load

function resetValues() {
  selectedTemps = [...defaultTemps];
  const dropdowns = document.querySelectorAll(".temperature-dropdown");
  dropdowns.forEach((dropdown, i) => {
      dropdown.value = selectedTemps[i];
      updateBackgroundColor(dropdown);
  });
}

