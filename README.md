# ⚡ SpikeStriker

> **"The 'Go to Sleep, Power is Expensive' Box."** 😴  
> Real-time price tracking to optimize energy consumption and save your wallet from peak-hour madness.

## 🛠 What is this?
A school IoT project designed to bridge the gap between live energy markets (pörssisähkö in Finland) and home appliances. Instead of manually checking prices, our hub does the thinking for you — optimizing when your 
devices run so you're always using the "cheapest" electrons available.

## 🤖 How does it work?
Operating Spike Striker is easy peasy lemon squeezy. Here is how to use it:

### 1. Adding a New Device (Pairing Mode)
  1. Open the Zigbee network via the UI.
  2. Wait until the Yellow LED starts blinking - this means the hub is hunting for new devices.
  3. Make sure your plug is in pairing mode and watch the magic happen.
     
https://github.com/user-attachments/assets/be181977-6a08-4ef0-acd4-ba78d705f8cb

Video 1: Hunting for new smart plugs in the wild. 

### 2. Toggling Devices via UI
Want to manually turn something on or off? You can toggle devices straight from the interface... assuming the electricity market permits your lifestyle choices. If a price threshold is violated, the system will respectfully decline your request.

https://github.com/user-attachments/assets/3a8e4d29-ae25-401a-a869-aec6a67d089f

Video 2: Toggling a plug (when the market allows it).

### 3. Updating Device Info & Priority
Gave your plug a promotion? You can update any device's name and assigned priority level directly in the settings menu

https://github.com/user-attachments/assets/2b2a0ed3-e625-4f07-b736-23ca9aa59911

Video 3: Renaming devices and assigning priority

### 4. Modifying Price Thresholds
Tired of living in the dark? Or feeling extra frugal today? You can adjust your low and medium price thresholds on the fly via the UI. Spike Striker will immediately recalculate which appliances deserve electricity.

https://github.com/user-attachments/assets/9a24d0ad-a1df-48f8-b79c-9db2f6440dd8

Video 4: Setting new threshold level

### 5. Automated Price Spike Execution (The "Market Police") 
When live electricity prices exceed your configured thresholds, Spike Striker takes immediate action, including strict override prevention: you cannot manually turn devices back on while price is spiking. 

https://github.com/user-attachments/assets/e40893bd-41bb-4a07-a981-18652280ea2b

Video 5: Spike Striker ruthlessly cutting power during a price peak. 

## 🦄 What Else?  (Features, Quirks & LED Guide) 

### 👻 Ghost Plugs (Disconnected Devices):
If a smart plug loses power or walks out of Zigbee range, the UI marks it as Disconnected so you aren't left guessing. 
<img width="1074" height="1152" alt="Näyttökuva 2026-10-11 162233" src="https://github.com/user-attachments/assets/e8c1df01-5288-4049-8be5-ab47b7749e60" />
Figure 1: UI showing a disconnected plug.

### 💥 Live Power Tracking: 
Keep an eye on your real-time wattage directly on the device screen. 
<img width="1138" height="1140" alt="Näyttökuva 2026-10-11 162500" src="https://github.com/user-attachments/assets/92dab228-e64b-4189-85f5-4c8c997e1bed" />
Figure 2: Live power consumption stats on the device screen. 

### 🛜 Wi-Fi Management:
Update your Wi-Fi credentials effortlessly in the settings tab. 
<img width="1096" height="1150" alt="Näyttökuva 2026-10-11 165320" src="https://github.com/user-attachments/assets/bbc58a04-d79a-4e03-9a41-371d90fc607b" />
Figure 3: Changing Wi-Fi credentials in the settings menu. 

## 🙀 Wait there is more! 

### 🗑️ Deleting a Device: 
You can remove a plug cleanly through the UI menu or if you factory reset the plug hardware itself, Spike Striker will automatically notice it's gone and scrub it from system memory. 

### 🚦The Official LED Status Survival Guide:
* **🟢 Solid Green:** Healthy, active system. Everything is fine and energy is flowing.
* **🟡 Blinking Yellow:** Network open - ready to pair new plugs.
* **🔴 Solid Red:** Inactive Zigbee network. If your plugs are currently plugged in during a red light emergency, a quick system reset is recommended! 

## ✨ Disclaimer & Credits
### ⚡"This is the way" (To lower your electircity bill) 
* **Spike Striker** takes zero responsibility for cold showers, unboiled tea or sudden darkness occurring during peak market hours. Blame Nord Pool, no the codebase.

< Built as an IoT Project to prove that saving money doesn't require Jedi wisdom - just smart endpoints, Zigbee network and low-cost electrons. 

