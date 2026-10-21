import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import { apply as applyTheme, watchSystem } from './os/theme.js'

// Design system first (tokens -> base -> components), then the self-hosted fonts.
import './assets/fonts.css'
import './design/tokens.css'
import './design/base.css'
import './design/components.css'

// Resolve dark/light/auto before first paint, and follow the OS while on auto.
applyTheme()
watchSystem()

createApp(App).use(createPinia()).mount('#app')
