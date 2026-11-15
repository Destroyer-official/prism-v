package com.prismv.ui

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            PrismTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    PrismDashboardScreen(
                        onCompressTriggered = { mode ->
                            Toast.makeText(
                                this,
                                "UI shell only: compression is not connected ($mode)",
                                Toast.LENGTH_SHORT
                            ).show()
                        }
                    )
                }
            }
        }
    }
}

enum class ParetoMode(val label: String, val icon: ImageVector) {
    TARGET_QUALITY("Target Quality", Icons.Default.HighQuality),
    TARGET_SIZE("Target Size", Icons.Default.Compress),
    MAX_SPEED("Max Speed", Icons.Default.Speed),
    BATTERY_SAVER("Battery Saver", Icons.Default.BatteryChargingFull)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PrismDashboardScreen(onCompressTriggered: (String) -> Unit) {
    var selectedMode by remember { mutableStateOf(ParetoMode.TARGET_QUALITY) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Prism-V Mobile Optimizer", fontSize = 20.sp) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = Color(0xFF0F172A),
                    titleContentColor = Color.White
                )
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Header card
            Card(
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1E293B)),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        "Prism-V",
                        color = Color(0xFF38BDF8),
                        fontSize = 14.sp
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        "Compose UI preview shell",
                        color = Color.White,
                        fontSize = 18.sp
                    )
                    Text(
                        "Status: compression and probing are not connected in this module.",
                        color = Color(0xFF94A3B8),
                        fontSize = 12.sp
                    )
                }
            }

            Text("Select Pareto Optimization Goal", color = Color.White, fontSize = 16.sp)

            // 4-Target Pareto Dial Selector
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                ParetoMode.values().forEach { mode ->
                    val isSelected = (selectedMode == mode)
                    Card(
                        shape = RoundedCornerShape(12.dp),
                        colors = CardDefaults.cardColors(
                            containerColor = if (isSelected) Color(0xFF0284C7) else Color(0xFF334155)
                        ),
                        modifier = Modifier
                            .weight(1f)
                            .clickable {
                                selectedMode = mode
                            }
                    ) {
                        Column(
                            modifier = Modifier.padding(12.dp),
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            Icon(mode.icon, contentDescription = null, tint = Color.White)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(mode.label, color = Color.White, fontSize = 11.sp, maxLines = 1)
                        }
                    }
                }
            }

            // 0.3s Preflight Pareto Probe Live Sheet
            Card(
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1E293B)),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Preflight Pareto Probe", color = Color.White, fontSize = 15.sp)
                        Badge(containerColor = Color(0xFF475569)) {
                            Text("Not measured", color = Color.White)
                        }
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        ProbeMetric(label = "Output Size", value = "n/a")
                        ProbeMetric(label = "Visual Quality", value = "n/a")
                        ProbeMetric(label = "Est. Time", value = "n/a")
                        ProbeMetric(label = "Battery Drain", value = "n/a")
                    }
                }
            }

            // Storage Deduplication Vault Banner
            Card(
                shape = RoundedCornerShape(12.dp),
                colors = CardDefaults.cardColors(containerColor = Color(0xFF0F766E)),
                modifier = Modifier.fillMaxWidth()
            ) {
                Row(
                    modifier = Modifier.padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(Icons.Default.FolderCopy, contentDescription = null, tint = Color.White)
                    Spacer(modifier = Modifier.width(8.dp))
                    Column {
                        Text("Rabin CDC Vault", color = Color.White, fontSize = 13.sp)
                        Text("Deduplication is not connected in this module.", color = Color(0xFFCCFBF1), fontSize = 11.sp)
                    }
                }
            }

            Spacer(modifier = Modifier.weight(1f))

            // Action Buttons
            Button(
                onClick = { onCompressTriggered(selectedMode.label) },
                shape = RoundedCornerShape(12.dp),
                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF0284C7)),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(52.dp)
            ) {
                Icon(Icons.Default.Bolt, contentDescription = null)
                Spacer(modifier = Modifier.width(8.dp))
                Text("Start Verified Compression", fontSize = 16.sp)
            }
        }
    }
}

@Composable
fun ProbeMetric(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(value, color = Color(0xFF38BDF8), fontSize = 16.sp)
        Text(label, color = Color(0xFF94A3B8), fontSize = 11.sp)
    }
}

@Composable
fun PrismTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            background = Color(0xFF0B1120),
            surface = Color(0xFF1E293B),
            primary = Color(0xFF0284C7)
        ),
        content = content
    )
}
