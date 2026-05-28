import { defineStore } from 'pinia'
import type { DebugStep } from '@/types/debug'

type NewStep = Omit<DebugStep, 'id' | 'status'>

export const useDebugStore = defineStore('debug', {
  state: () => ({
    steps: [] as DebugStep[]
  }),
  actions: {
    addStep(step: NewStep) {
      const id = `${Date.now()}-${Math.random().toString(16).slice(2)}`
      this.steps.push({ id, status: 'pending', ...step })
      return id
    },
    startStep(id: string) {
      const step = this.steps.find((item) => item.id === id)
      if (step) {
        step.status = 'running'
        step.startedAt = Date.now()
      }
    },
    finishStep(id: string, output?: unknown, response?: unknown) {
      const step = this.steps.find((item) => item.id === id)
      if (step) {
        step.status = 'success'
        step.output = output
        step.response = response
        step.endedAt = Date.now()
        step.durationMs = step.startedAt ? step.endedAt - step.startedAt : undefined
      }
    },
    failStep(id: string, error: string, extra?: Partial<DebugStep>) {
      const step = this.steps.find((item) => item.id === id)
      if (step) {
        step.status = 'error'
        step.error = error
        step.endedAt = Date.now()
        step.durationMs = step.startedAt ? step.endedAt - step.startedAt : extra?.durationMs
        Object.assign(step, extra)
      }
    },
    clearSteps() {
      this.steps = []
    },
    exportTraceJson() {
      return JSON.stringify(this.steps, null, 2)
    }
  }
})
