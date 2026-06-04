import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Xpress++',
  description: 'Express-style C++ web framework documentation',
  cleanUrls: true,
  appearance: true,
  lastUpdated: true,
  markdown: {
    lineNumbers: true
  },
  head: [
    ['meta', { name: 'theme-color', content: '#0b0f19' }]
  ],
  themeConfig: {
    logo: { light: '/logo-light.svg', dark: '/logo-dark.svg' },
    siteTitle: 'Xpress++',
    search: {
      provider: 'local'
    },
    nav: [
      { text: 'Guides', link: '/getting-started' },
      { text: 'Reference', link: '/request' },
      { text: 'CLI', link: '/cli' }
    ],
    sidebar: [
      {
        text: 'Get Started',
        items: [
          { text: 'Overview', link: '/' },
          { text: 'Getting started', link: '/getting-started' },
          { text: 'Project structure', link: '/project-structure' }
        ]
      },
      {
        text: 'Framework',
        items: [
          { text: 'Routing', link: '/routing' },
          { text: 'Request', link: '/request' },
          { text: 'Response', link: '/response' },
          { text: 'Middleware', link: '/middleware' },
          { text: 'Error handling', link: '/error-handling' },
          { text: 'Configuration', link: '/configuration' },
          { text: 'File uploads', link: '/file-uploads' }
        ]
      },
      {
        text: 'Built-ins',
        items: [
          { text: 'CORS', link: '/cors' },
          { text: 'Rate limiting', link: '/rate-limit' },
          { text: 'Security', link: '/security' },
          { text: 'Authentication', link: '/authentication' },
          { text: 'Static files', link: '/static-files' },
          { text: 'JSON', link: '/json' }
        ]
      },
      {
        text: 'Tooling',
        items: [
          { text: 'xp CLI', link: '/cli' },
          { text: 'Dependencies', link: '/dependencies' },
          { text: 'Next steps', link: '/next-steps' }
        ]
      }
    ],
    footer: {
      message: 'Built for scalable Xpress++ documentation.',
      copyright: 'Copyright © 2026 Xpress++'
    }
  }
})
