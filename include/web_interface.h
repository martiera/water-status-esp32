#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Water Status Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            padding: 30px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            text-align: center;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 30px;
        }
        .section {
            margin-bottom: 30px;
            padding: 20px;
            background: #f8f9fa;
            border-radius: 8px;
        }
        .section h2 {
            color: #667eea;
            margin-bottom: 15px;
            font-size: 18px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #555;
            font-weight: 500;
        }
        input[type="text"],
        input[type="password"],
        input[type="number"] {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        .btn {
            width: 100%;
            padding: 12px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s;
        }
        .btn:hover {
            transform: translateY(-2px);
        }
        .btn:active {
            transform: translateY(0);
        }
        .status {
            padding: 15px;
            margin-top: 20px;
            border-radius: 5px;
            text-align: center;
            display: none;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .current-temps {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 15px;
        }
        .temp-box {
            background: white;
            padding: 15px;
            border-radius: 5px;
            text-align: center;
        }
        .temp-label {
            font-size: 12px;
            color: #666;
            margin-bottom: 5px;
        }
        .temp-value {
            font-size: 24px;
            font-weight: bold;
            color: #667eea;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>💧 Water Status Monitor</h1>
        <p class="subtitle">Configuration Panel</p>
        
        <form id="configForm">
            <!-- WiFi Settings -->
            <div class="section">
                <h2>📶 WiFi Settings</h2>
                <div class="form-group">
                    <label for="wifi_ssid">SSID:</label>
                    <input type="text" id="wifi_ssid" name="wifi_ssid" required>
                </div>
                <div class="form-group">
                    <label for="wifi_password">Password:</label>
                    <input type="password" id="wifi_password" name="wifi_password" required>
                </div>
            </div>
            
            <!-- MQTT Settings -->
            <div class="section">
                <h2>📡 MQTT Broker</h2>
                <div class="form-group">
                    <label for="mqtt_server">Server:</label>
                    <input type="text" id="mqtt_server" name="mqtt_server" required>
                </div>
                <div class="form-group">
                    <label for="mqtt_port">Port:</label>
                    <input type="number" id="mqtt_port" name="mqtt_port" value="1883" required>
                </div>
                <div class="form-group">
                    <label for="mqtt_user">Username (optional):</label>
                    <input type="text" id="mqtt_user" name="mqtt_user">
                </div>
                <div class="form-group">
                    <label for="mqtt_password">Password (optional):</label>
                    <input type="password" id="mqtt_password" name="mqtt_password">
                </div>
            </div>
            
            <!-- MQTT Topics -->
            <div class="section">
                <h2>🔖 MQTT Topics</h2>
                <div class="form-group">
                    <label for="topic_tank">Tank Temperature:</label>
                    <input type="text" id="topic_tank" name="topic_tank" value="home/water/tank/temperature" required>
                </div>
                <div class="form-group">
                    <label for="topic_out">Out Pipe Temperature:</label>
                    <input type="text" id="topic_out" name="topic_out" value="home/water/outpipe/temperature" required>
                </div>
                <div class="form-group">
                    <label for="topic_heat_in">Heating Incoming:</label>
                    <input type="text" id="topic_heat_in" name="topic_heat_in" value="home/water/heating/in/temperature" required>
                </div>
                <div class="form-group">
                    <label for="topic_heat_out">Heating Exiting:</label>
                    <input type="text" id="topic_heat_out" name="topic_heat_out" value="home/water/heating/out/temperature" required>
                </div>
            </div>
            
            <!-- Temperature Thresholds -->
            <div class="section">
                <h2>🌡️ Bath Ready Thresholds (°C)</h2>
                <div class="form-group">
                    <label for="min_tank">Minimum Tank Temperature:</label>
                    <input type="number" id="min_tank" name="min_tank" value="52" step="0.5" required>
                </div>
                <div class="form-group">
                    <label for="min_out">Minimum Out Pipe Temperature:</label>
                    <input type="number" id="min_out" name="min_out" value="38" step="0.5" required>
                </div>
            </div>
            
            <button type="submit" class="btn">💾 Save Configuration</button>
        </form>
        
        <div id="status" class="status"></div>
    </div>
    
    <script>
        document.getElementById('configForm').addEventListener('submit', async (e) => {
            e.preventDefault();
            
            const formData = new FormData(e.target);
            const data = Object.fromEntries(formData.entries());
            
            const statusDiv = document.getElementById('status');
            statusDiv.style.display = 'block';
            statusDiv.className = 'status';
            statusDiv.textContent = 'Saving configuration...';
            
            try {
                const response = await fetch('/save', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(data)
                });
                
                if (response.ok) {
                    statusDiv.className = 'status success';
                    statusDiv.textContent = '✓ Configuration saved! Device will restart in 3 seconds...';
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 3000);
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.textContent = '✗ Failed to save configuration';
                }
            } catch (error) {
                statusDiv.className = 'status error';
                statusDiv.textContent = '✗ Error: ' + error.message;
            }
        });
        
        // Load current configuration
        async function loadConfig() {
            try {
                const response = await fetch('/config');
                if (response.ok) {
                    const config = await response.json();
                    Object.keys(config).forEach(key => {
                        const input = document.getElementById(key);
                        if (input) input.value = config[key];
                    });
                }
            } catch (error) {
                console.error('Failed to load config:', error);
            }
        }
        
        loadConfig();
    </script>
</body>
</html>
)rawliteral";

#endif
