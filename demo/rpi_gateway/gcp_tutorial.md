# IoT Exploratory 2026: Google Cloud Setup Guide

**Architecture Overview:**
Raspberry Pi (Edge Gateway) -> Cloud Pub/Sub -> Cloud Functions -> BigQuery -> Looker Studio (Dashboard).

---

## PART 1: ORGANIZER PRE-WORKSHOP SETUP
*This section is to be completed by the instructor/organizer before the workshop begins to avoid billing friction for attendees.*

### 1. Create the Master Project
1. Go to the [Google Cloud Console](https://console.cloud.google.com/).
2. Click the **Project Dropdown** > **New Project**.
3. Name it: `iot-exploratory-2026`. (**Make note of the exact Project ID**, as Google may append numbers to the end, e.g., `iot-exploratory-2026-12345`).
4. Ensure billing is enabled for this project.

### 2. Enable Required APIs
In the Cloud Console search bar, search for **APIs & Services**, click **Enable APIs and Services**, and turn on the following one by one:
* **Cloud Pub/Sub API**
* **BigQuery API**
* **Cloud Firestore API**
* **Cloud Functions API**
* **Cloud Build API**
* **Cloud Run Admin API**
* **IAM Service Account Credentials API**

Google Cloud CLI has a built-in --filter flag that works perfectly on Windows. This command will ask Google Cloud to list only the specific APIs we need for the workshop and tell you if they are enabled.

Copy and paste this exact block into your terminal and press Enter:
```bash
gcloud services list --enabled --filter="name:pubsub.googleapis.com OR name:bigquery.googleapis.com OR firestore.googleapis.com OR name:cloudfunctions.googleapis.com OR name:cloudbuild.googleapis.com OR name:run.googleapis.com OR name:iamcredentials.googleapis.com"
```

What to look for:
If everything is set up correctly, the terminal will return a neat table that looks exactly like this:
```plaintext
NAME                               TITLE
bigquery.googleapis.com            BigQuery API
cloudbuild.googleapis.com          Cloud Build API
cloudfunctions.googleapis.com      Cloud Functions API
firestore.googleapis.com           Cloud Firestore API
iamcredentials.googleapis.com      IAM Service Account Credentials API
pubsub.googleapis.com              Cloud Pub/Sub API
run.googleapis.com                 Cloud Run Admin API
```

If any of the APIs are missing from that list, it means they aren't enabled yet. You can actually enable them right there from the command line without going back to the web browser by typing:
`gcloud services enable [API_NAME]` (e.g., `gcloud services enable cloudbuild.googleapis.com`).

### 3. Grant Access to Attendees
1. Navigate to **IAM & Admin** > **IAM**.
2. Click **Grant Access** (or Add).
3. Under **New principals**, paste the Google account email addresses of all your attendees.
4. Under **Assign roles**, grant them the **Editor** role. 
5. Click **Save**. Attendees will automatically receive an email invitation and have access to the project.

Here is the gcloud CLI command to grant the Editor role to your attendees. Since you are likely adding multiple students, you can either do it one by one or use a simple script to add a whole list of them at once.

#### Option 1: Add a single attendee
Run this command, replacing PROJECT_ID with your project ID and the email with the student's actual Google account email:

```bash
gcloud projects add-iam-policy-binding iot-exploratory-2026 \
    --member="user:student1@gmail.com" \
    --role="roles/editor"
```

#### Option 2: Add multiple attendees at once (Best for Workshops)
If you have a list of attendees, you can use a simple Bash loop in your WSL/Linux terminal to add them all in one go.

Copy this block, replace the emails in the ATTENDEES list, and run it:

```bash
# 1. Define your project ID
PROJECT_ID="iot-exploratory-2026"

# 2. List all attendee emails separated by a space
ATTENDEES="student1@gmail.com student2@gmail.com student3@university.edu"

# 3. Loop through and grant the Editor role to each
for EMAIL in $ATTENDEES; do
    echo "Granting Editor role to $EMAIL..."
    gcloud projects add-iam-policy-binding $PROJECT_ID \
        --member="user:$EMAIL" \
        --role="roles/editor" \
        --condition=None
done

echo "All attendees have been granted access!"
```

Note: Just like in the web console, Google Cloud will automatically send an email invitation to each user when you run this command.


---

## PART 2: ATTENDEE WORKSHOP LAB
*Welcome! In this lab, you will use your Raspberry Pi to build the cloud pipeline for your IoT system. Open your Raspberry Pi terminal to begin.*

### Step 1: Install Google Cloud CLI on Raspberry Pi OS
First, we need to install the `gcloud` command-line tool. Run these commands one by one to add the Google Cloud repository to your Raspberry Pi:

```bash
# 1. Install prerequisites
sudo apt-get update
sudo apt-get install apt-transport-https ca-certificates gnupg curl -y

# 2. Import the Google Cloud public key
curl [https://packages.cloud.google.com/apt/doc/apt-key.gpg](https://packages.cloud.google.com/apt/doc/apt-key.gpg) | sudo gpg --dearmor -o /usr/share/keyrings/cloud.google.gpg

# 3. Add the gcloud CLI distribution URI as a package source
echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] [https://packages.cloud.google.com/apt](https://packages.cloud.google.com/apt) cloud-sdk main" | sudo tee -a /etc/apt/sources.list.d/google-cloud-sdk.list

# 4. Update and install the CLI
sudo apt-get update && sudo apt-get install google-cloud-cli -y
```

### Step 2: Authenticate and Set Up Your Environment
Now, log in to Google Cloud and set your variables so the rest of the commands run automatically for your specific group.

```bash
# Log in to your Google Account (Follow the link provided in the terminal, login, and paste the code back)
gcloud auth login

# SET YOUR WORKSHOP VARIABLES HERE
# Replace 'group1' with your assigned group name!
export GROUP_NAME="group1"
# Replace with the exact Project ID provided by your instructor!
export PROJECT_ID="iot-exploratory-2026"

# Set the active project for this terminal session
gcloud config set project $PROJECT_ID
```

### Step 3: Create Pub/Sub Topics
Pub/Sub acts as the messaging bridge between your Pi and the Cloud. Create your isolated topics to prevent your data from mixing with other groups:

```bash
gcloud pubsub topics create $GROUP_NAME-telemetry-up
gcloud pubsub topics create $GROUP_NAME-commands-down
```

### Step 4: Configure BigQuery (Time-Series Storage)
BigQuery will store every telemetry packet and status update for historical visualization in Looker Studio.

```bash
# Create your group's dataset
bq mk --dataset $PROJECT_ID:${GROUP_NAME}

# Create the sensor_data table with JSON payload support
bq mk --table $PROJECT_ID:${GROUP_NAME}.sensor_data \
timestamp:TIMESTAMP,device_id:STRING,message_id:STRING,packet_type:STRING,payload:JSON

# Create the actuator_data table
bq mk --table $PROJECT_ID:${GROUP_NAME}.actuator_data \
timestamp:TIMESTAMP,device_id:STRING,message_id:STRING,packet_type:STRING,payload:JSON
```

### Step 5: Configure Firestore (Real-Time Database)
While BigQuery stores your history for charts, Firestore will store the *current* real-time state of your sensors and actuators so your web dashboard updates instantly.

Run this command to initialize your Firestore database:

```bash
# Create the default Firestore database in the Asia region
gcloud firestore databases create \
    --project=$PROJECT_ID \
    --database=$GROUP_NAME \
    --location=asia-southeast3 \
    --type=firestore-native
```

### Step 6: Create a Service Account for Your Raspberry Pi
Your Raspberry Pi needs a secure identity key to publish and receive data automatically without requiring a human to log in.

```bash
# 1. Create the Service Account
gcloud iam service-accounts create ${GROUP_NAME}-pi-gateway \
    --display-name="Gateway for ${GROUP_NAME}"

# 2. Grant it permission to publish sensor data
gcloud projects add-iam-policy-binding $PROJECT_ID \
    --member="serviceAccount:${GROUP_NAME}-pi-gateway@${PROJECT_ID}.iam.gserviceaccount.com" \
    --role="roles/pubsub.publisher"
    
# 3. Grant it permission to subscribe to cloud commands
gcloud projects add-iam-policy-binding $PROJECT_ID \
    --member="serviceAccount:${GROUP_NAME}-pi-gateway@${PROJECT_ID}.iam.gserviceaccount.com" \
    --role="roles/pubsub.subscriber"

# 4. Download the security key directly to your Pi's home directory!
gcloud iam service-accounts keys create ~/${GROUP_NAME}-key.json \
    --iam-account=${GROUP_NAME}-pi-gateway@${PROJECT_ID}.iam.gserviceaccount.com

echo "SUCCESS! Your security key has been saved to ~/${GROUP_NAME}-key.json"
```

Keep this .json file safe! Your Python gateway script will use it to authenticate to Google Cloud.

### Step 7: Deploy the Cloud Function
(Wait for your instructor to provide the Python main.py and requirements.txt code for the data parser. Once you have saved those files on your Pi, you will run the deployment command below).

```bash
# Deployment command (Do not run until you have written main.py!)
# gcloud functions deploy ${GROUP_NAME}-pubsub-to-bq \
#  --gen2 \
#  --runtime=python311 \
#  --region=asia-southeast3 \
#  --source=. \
#  --entry-point=process_mqtt_message \
#  --trigger-topic=${GROUP_NAME}-telemetry-up
```

